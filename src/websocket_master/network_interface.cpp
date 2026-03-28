/*
 * network_interface.cpp — epoll 主循环与非阻塞 I/O
 *
 * 读：循环 read 至 EAGAIN，再 drive_io 解析握手/帧。
 * 写：出站缓冲 + EPOLLOUT；关闭通过 queue_close 延迟到本轮事件末尾处理。
 */

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif
#include <sys/epoll.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <algorithm>
#include "debug_log.h"
#include "network_interface.h"

Network_Interface *Network_Interface::m_network_interface = NULL;

Network_Interface::Network_Interface():
		epollfd_(0),
		listenfd_(0),
		websocket_handler_map_(),
		ws_clients_(),
		pending_closes_(),
		epoll_events_buf_()
{
	if(0 != init())
		exit(1);
}

Network_Interface::~Network_Interface(){}

void Network_Interface::raise_nofile_limit_early(){
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
		return;
	rlim_t want = (rlim_t)TARGET_MIN_OPEN_FILES;
	if (rl.rlim_max != RLIM_INFINITY && want > rl.rlim_max)
		want = rl.rlim_max;
	if (rl.rlim_cur < want) {
		rl.rlim_cur = want;
		if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
			if (rl.rlim_max != RLIM_INFINITY)
				rl.rlim_cur = rl.rlim_max;
			setrlimit(RLIMIT_NOFILE, &rl);
		}
	}
}

int Network_Interface::tune_listen_socket(){
	int one = 1;
	if (setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
		DEBUG_LOG("SO_REUSEADDR failed errno=%d", errno);
		return -1;
	}
	if (setsockopt(listenfd_, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) != 0) {
		DEBUG_LOG("SO_REUSEPORT failed errno=%d", errno);
		return -1;
	}
	return 0;
}

int Network_Interface::tune_client_socket(int fd){
	int one = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0)
		DEBUG_LOG("TCP_NODELAY failed fd=%d errno=%d", fd, errno);
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) != 0)
		DEBUG_LOG("SO_KEEPALIVE failed fd=%d errno=%d", fd, errno);
	return 0;
}

int Network_Interface::init(){
	raise_nofile_limit_early();

	listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd_ == -1){
		DEBUG_LOG("创建套接字失败!");
		return -1;
	}
	if (tune_listen_socket() != 0) {
		close(listenfd_);
		return -1;
	}
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);
	if(-1 == bind(listenfd_, (struct sockaddr *)(&server_addr), sizeof(server_addr))){
		DEBUG_LOG("绑定套接字失败!");
		close(listenfd_);
		return -1;
	}
	if(-1 == listen(listenfd_, LISTEN_BACKLOG)){
		DEBUG_LOG("监听失败!");
		close(listenfd_);
		return -1;
	}

	epollfd_ = epoll_create1(EPOLL_CLOEXEC);
	if (epollfd_ < 0) {
		epollfd_ = epoll_create(1024);
		if (epollfd_ < 0) {
			DEBUG_LOG("epoll_create 失败 errno=%d", errno);
			close(listenfd_);
			return -1;
		}
	}

	epoll_events_buf_.resize(MAX_EPOLL_EVENTS_PER_WAIT);

	ctl_event(listenfd_, true);
	DEBUG_LOG("服务器启动成功! (listen backlog=%d epoll_batch=%zu)",
			LISTEN_BACKLOG, epoll_events_buf_.size());
	return 0;
}

void Network_Interface::mod_events(int fd, uint32_t events){
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = events;
	if (epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &ev) == -1)
		DEBUG_LOG("epoll_ctl MOD failed fd=%d errno=%d", fd, errno);
}

void Network_Interface::update_handler_events(int fd){
	if (fd == listenfd_)
		return;
	WEB_SOCKET_HANDLER_MAP::iterator it = websocket_handler_map_.find(fd);
	if (it == websocket_handler_map_.end() || it->second == NULL)
		return;
	uint32_t ev = EPOLLIN;
	if (it->second->write_pending())
		ev |= EPOLLOUT;
	mod_events(fd, ev);
}

bool Network_Interface::is_listen_fd(int fd) const{
	return fd == listenfd_;
}

void Network_Interface::register_ws_client(int fd){
	if (fd == listenfd_ || fd < 0)
		return;
	ws_clients_.insert(fd);
}

void Network_Interface::unregister_ws_client(int fd){
	ws_clients_.erase(fd);
}

void Network_Interface::broadcast_ws_binary(int except_fd, const void *data, size_t len){
	/* append_outgoing(..., false) 避免每连接一次 epoll_ctl；最后只对“由空变非空”的 fd MOD */
	if (!data || len == 0)
		return;
	std::vector<int> to_refresh;
	to_refresh.reserve(256);
	for (std::unordered_set<int>::const_iterator it = ws_clients_.begin(); it != ws_clients_.end(); ++it) {
		int cfd = *it;
		if (cfd == except_fd)
			continue;
		WEB_SOCKET_HANDLER_MAP::iterator hi = websocket_handler_map_.find(cfd);
		if (hi == websocket_handler_map_.end() || hi->second == NULL)
			continue;
		Websocket_Handler *h = hi->second;
		const bool was_empty = !h->write_pending();
		h->append_outgoing(data, len, false);
		if (was_empty && h->write_pending())
			to_refresh.push_back(cfd);
	}
	for (size_t i = 0; i < to_refresh.size(); ++i)
		update_handler_events(to_refresh[i]);
}

void Network_Interface::queue_close(int fd){
	if (fd < 0)
		return;
	pending_closes_.push_back(fd);
}

void Network_Interface::process_close_queue(){
	if (pending_closes_.empty())
		return;
	std::sort(pending_closes_.begin(), pending_closes_.end());
	pending_closes_.erase(std::unique(pending_closes_.begin(), pending_closes_.end()), pending_closes_.end());
	for (size_t i = 0; i < pending_closes_.size(); ++i) {
		int fd = pending_closes_[i];
		if (websocket_handler_map_.find(fd) != websocket_handler_map_.end())
			ctl_event(fd, false);
	}
	pending_closes_.clear();
}

int Network_Interface::epoll_loop(){
	struct sockaddr_in client_addr;
	socklen_t clilen = sizeof(client_addr);
	int nfds = 0;
	int fd = 0;
	char chunk[READ_CHUNK];

	while(true){
		do {
			nfds = epoll_wait(epollfd_, epoll_events_buf_.data(),
					static_cast<int>(epoll_events_buf_.size()), EPOLL_WAIT_MS);
		} while (nfds < 0 && errno == EINTR);
		if (nfds < 0) {
			DEBUG_LOG("epoll_wait 失败 errno=%d", errno);
			continue;
		}
		for(int i = 0; i < nfds; i++){
			if(epoll_events_buf_[i].data.fd == listenfd_){
				for (;;) {
					fd = accept(listenfd_, (struct sockaddr *)&client_addr, &clilen);
					if (fd < 0) {
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;
						if (errno == EMFILE || errno == ENFILE)
							DEBUG_LOG("accept 拒绝: 文件描述符耗尽 errno=%d", errno);
						else
							DEBUG_LOG("accept 失败 errno=%d", errno);
						break;
					}
					tune_client_socket(fd);
					ctl_event(fd, true);
				}
			}
			else if(epoll_events_buf_[i].events & (EPOLLERR | EPOLLHUP)){
				fd = epoll_events_buf_[i].data.fd;
				queue_close(fd);
			}
			else if(epoll_events_buf_[i].events & EPOLLIN){
				fd = epoll_events_buf_[i].data.fd;
				if (fd == listenfd_)
					continue;
				WEB_SOCKET_HANDLER_MAP::iterator hit = websocket_handler_map_.find(fd);
				if (hit == websocket_handler_map_.end() || hit->second == NULL)
					continue;
				Websocket_Handler *handler = hit->second;

				bool close_me = false;
				for (;;) {
					ssize_t n = read(fd, chunk, sizeof(chunk));
					if (n > 0) {
						if (!handler->append_read(chunk, static_cast<size_t>(n))) {
							DEBUG_LOG("读缓冲上限 fd=%d", fd);
							close_me = true;
							break;
						}
					} else if (n == 0) {
						close_me = true;
						break;
					} else {
						if (errno == EAGAIN || errno == EWOULDBLOCK)
							break;
						close_me = true;
						break;
					}
				}
				if (close_me) {
					queue_close(fd);
					continue;
				}
				handler->drive_io();
			}
			else if(epoll_events_buf_[i].events & EPOLLOUT){
				fd = epoll_events_buf_[i].data.fd;
				if (fd == listenfd_)
					continue;
				WEB_SOCKET_HANDLER_MAP::iterator hit = websocket_handler_map_.find(fd);
				if (hit == websocket_handler_map_.end() || hit->second == NULL)
					continue;
				Websocket_Handler *handler = hit->second;
				handler->flush_writes();
				update_handler_events(fd);
			}
		}
		process_close_queue();
	}

	return 0;
}

int Network_Interface::set_noblock(int fd){
	int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

Network_Interface *Network_Interface::get_share_network_interface(){
	if(m_network_interface == NULL)
		m_network_interface = new Network_Interface();
	return m_network_interface;
}

void Network_Interface::ctl_event(int fd, bool flag){
	if (!flag) {
		if (websocket_handler_map_.find(fd) == websocket_handler_map_.end())
			return;
		unregister_ws_client(fd);
	}
	struct epoll_event ev;
	ev.data.fd = fd;
	ev.events = flag ? EPOLLIN : 0;
	epoll_ctl(epollfd_, flag ? EPOLL_CTL_ADD : EPOLL_CTL_DEL, fd, &ev);
	if(flag){
		set_noblock(fd);
		websocket_handler_map_[fd] = new Websocket_Handler(fd);
	}
	else{
		close(fd);
		delete websocket_handler_map_[fd];
		websocket_handler_map_.erase(fd);
	}
}

void Network_Interface::run(){
	epoll_loop();
}
