#ifndef __NETWORK_INTERFACE__
#define __NETWORK_INTERFACE__

/*
 * network_interface.h — epoll 事件循环、监听与连接管理
 *
 * 职责概要：
 * - 每个进程内一个 Network_Interface：独立 listen + epoll + 连接表
 * - 多进程模式（main fork + SO_REUSEPORT）下，由内核把新连接分到各 worker
 * - 广播仅在本进程已注册客户端集合 ws_clients_ 内进行（跨进程需另加消息总线）
 */

#include <stdint.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "websocket_handler.h"

#define PORT                         9000
#define EPOLL_WAIT_MS                (-1)   /* epoll_wait 超时：-1 表示阻塞直至有事件 */
#define READ_CHUNK                   16384  /* 单次 read 块大小 */
#define MAX_EPOLL_EVENTS_PER_WAIT    8192   /* 每轮 epoll_wait 最多返回事件数 */
#define LISTEN_BACKLOG               4096   /* listen 队列长度 */
#define TARGET_MIN_OPEN_FILES        65536  /* 尝试抬高的 RLIMIT_NOFILE 目标 */

typedef std::unordered_map<int, Websocket_Handler *> WEB_SOCKET_HANDLER_MAP;

class Network_Interface {
private:
	Network_Interface();
	~Network_Interface();
	int     init();
	int     epoll_loop();
	int     set_noblock(int fd);
	int     tune_listen_socket();   /* SO_REUSEADDR + SO_REUSEPORT（多 worker 绑定同一端口） */
	int     tune_client_socket(int fd);
	void    ctl_event(int fd, bool flag);
	void    mod_events(int fd, uint32_t events);
	void    process_close_queue();  /* 本轮事件处理结束后再关连接，避免在 handler 内 delete this */
public:
	void    run();
	static  Network_Interface *get_share_network_interface();
	/* 在 fork 子进程前由父进程调用，尽量提高 fd 上限；子进程会继承 */
	static  void raise_nofile_limit_early();
	void    queue_close(int fd);
	void    update_handler_events(int fd);
	bool    is_listen_fd(int fd) const;
	void    register_ws_client(int fd);      /* WebSocket 握手成功后加入广播集合 */
	void    unregister_ws_client(int fd);
	/* 向除 except_fd 外的本进程客户端写同一帧；批量 append 后统一 epoll MOD */
	void    broadcast_ws_binary(int except_fd, const void *data, size_t len);
private:
	int                         epollfd_;
	int                         listenfd_;
	WEB_SOCKET_HANDLER_MAP      websocket_handler_map_;
	std::unordered_set<int>     ws_clients_;
	std::vector<int>            pending_closes_;
	std::vector<epoll_event>    epoll_events_buf_;
	static Network_Interface*   m_network_interface;
};

#define NETWORK_INTERFACE Network_Interface::get_share_network_interface()

#endif
