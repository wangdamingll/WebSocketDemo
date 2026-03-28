/*
 * websocket_handler.cpp — 单连接：HTTP 升级握手、帧解析、回显与广播
 *
 * 握手完成并发出 101 后 register_ws_client；析构时 unregister。
 * append_outgoing(..., update_epoll=false) 供 Network_Interface 批量广播时减少 epoll 抖动。
 */

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <cstring>
#include "websocket_handler.h"
#include "network_interface.h"

#define WS_MAX_HANDSHAKE_BYTES (64 * 1024)
#define WS_MAX_READ_BUFFER     (1024 * 1024 + 16 * 1024) /* >= 单帧最大 payload，见 websocket_request.h */
#define WS_MAX_WRITE_BUFFER    (4 * 1024 * 1024)

Websocket_Handler::Websocket_Handler(int fd):
	read_buf_(),
	write_buf_(),
	status_(WEBSOCKET_UNCONNECT),
	header_map_(),
	fd_(fd),
	request_(new Websocket_Request),
	respond_(new Websocket_Respond)
{
}

Websocket_Handler::~Websocket_Handler(){
	NETWORK_INTERFACE->unregister_ws_client(fd_);
	delete request_;
	delete respond_;
}

bool Websocket_Handler::append_read(const void *data, size_t len){
	if (!data || len == 0)
		return true;
	if (read_buf_.size() + len > WS_MAX_READ_BUFFER)
		return false;
	const char *p = static_cast<const char *>(data);
	read_buf_.insert(read_buf_.end(), p, p + len);
	return true;
}

void Websocket_Handler::append_outgoing(const void *data, size_t len, bool update_epoll){
	/* update_epoll=false 时由调用方（如 broadcast_ws_binary）统一刷新监听掩码 */
	if (!data || len == 0)
		return;
	if (write_buf_.size() + len > WS_MAX_WRITE_BUFFER) {
		NETWORK_INTERFACE->queue_close(fd_);
		return;
	}
	write_buf_.append(static_cast<const char *>(data), len);
	if (update_epoll)
		NETWORK_INTERFACE->update_handler_events(fd_);
}

void Websocket_Handler::queue_send(const void *data, size_t len){
	append_outgoing(data, len, true);
}

void Websocket_Handler::flush_writes(){
	while (!write_buf_.empty()) {
		ssize_t n = ::send(fd_, write_buf_.data(), write_buf_.size(), MSG_NOSIGNAL);
		if (n > 0)
			write_buf_.erase(0, static_cast<size_t>(n));
		else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break;
		else {
			NETWORK_INTERFACE->queue_close(fd_);
			return;
		}
	}
}

bool Websocket_Handler::write_pending() const{
	return !write_buf_.empty();
}

void Websocket_Handler::drive_io(){
	if (status_ == WEBSOCKET_UNCONNECT) {
		if (read_buf_.size() > WS_MAX_HANDSHAKE_BYTES) {
			NETWORK_INTERFACE->queue_close(fd_);
			return;
		}
		std::string s(read_buf_.begin(), read_buf_.end());
		size_t pos = s.find("\r\n\r\n");
		if (pos == std::string::npos)
			return;

		std::string headers = s.substr(0, pos + 4);
		read_buf_.erase(read_buf_.begin(), read_buf_.begin() + static_cast<std::ptrdiff_t>(pos + 4));

		if (fetch_http_info(headers) != 0) {
			NETWORK_INTERFACE->queue_close(fd_);
			return;
		}
		status_ = WEBSOCKET_HANDSHARKED;
		char response[1024] = {};
		parse_str(response);
		queue_send(response, strlen(response));
		if (!NETWORK_INTERFACE->is_listen_fd(fd_))
			NETWORK_INTERFACE->register_ws_client(fd_);
	}

	while (status_ == WEBSOCKET_HANDSHARKED) {
		int pr = request_->try_consume_frame(read_buf_);
		if (pr == WS_PARSE_NEED_MORE)
			break;
		if (pr == WS_PARSE_ERROR) {
			NETWORK_INTERFACE->queue_close(fd_);
			return;
		}
		handle_one_frame();
		request_->reset();
	}
}

void Websocket_Handler::handle_one_frame(){
	unsigned char *out = NULL;
	size_t out_len = 0;
	std::string request_msg = request_->get_msg();
	size_t request_msg_len = static_cast<size_t>(request_->get_msg_length());
	uint8_t msg_opcode = request_->get_msg_opcode_();

	if (msg_opcode == WEBSOCKET_TEXT_DATA) {
		if (respond_->pack_data(
				reinterpret_cast<const unsigned char *>(request_msg.data()),
				request_msg_len,
				WEBSOCKET_FIN_MSG_END,
				WEBSOCKET_TEXT_DATA,
				WEBSOCKET_NEED_NOT_MASK,
				&out,
				&out_len) != 0) {
			if (out)
				free(out);
			return;
		}
		queue_send(out, out_len);
		NETWORK_INTERFACE->broadcast_ws_binary(fd_, out, out_len);
		free(out);
	} else if (msg_opcode == WEBSOCKET_CONNECT_CLOSE) {
		NETWORK_INTERFACE->queue_close(fd_);
	} else if (msg_opcode == WEBSOCKET_PING_PACK) {
		unsigned char *pong = NULL;
		size_t pong_len = 0;
		if (respond_->pack_data(
				reinterpret_cast<const unsigned char *>(request_msg.data()),
				request_msg_len,
				WEBSOCKET_FIN_MSG_END,
				WEBSOCKET_PANG_PACK,
				WEBSOCKET_NEED_NOT_MASK,
				&pong,
				&pong_len) == 0 && pong) {
			queue_send(pong, pong_len);
			free(pong);
		}
	}
}

void Websocket_Handler::parse_str(char *request){
	strcat(request, "HTTP/1.1 101 Switching Protocols\r\n");
	strcat(request, "Connection: upgrade\r\n");
	strcat(request, "Sec-WebSocket-Accept: ");
	std::string server_key = header_map_["Sec-WebSocket-Key"];
	server_key += MAGIC_KEY;

	SHA1 sha;
	unsigned int message_digest[5];
	sha.Reset();
	sha << server_key.c_str();

	sha.Result(message_digest);
	for (int i = 0; i < 5; i++) {
		message_digest[i] = htonl(message_digest[i]);
	}
	server_key = base64_encode(reinterpret_cast<const unsigned char*>(message_digest), 20);
	server_key += "\r\n";
	strcat(request, server_key.c_str());
	strcat(request, "Upgrade: websocket\r\n\r\n");
}

int Websocket_Handler::fetch_http_info(const std::string &raw_headers){
	std::istringstream s(raw_headers);
	std::string request;

	if (!std::getline(s, request))
		return -1;
	if (request.empty())
		return -1;
	if (request[request.size() - 1] == '\r') {
		request.erase(request.end() - 1);
	} else {
		return -1;
	}

	std::string header;
	std::string::size_type end;

	while (std::getline(s, header) && header != "\r") {
		if (header.empty())
			continue;
		if (header[header.size() - 1] != '\r') {
			continue;
		} else {
			header.erase(header.end() - 1);
		}

		end = header.find(": ", 0);
		if (end != std::string::npos) {
			std::string key = header.substr(0, end);
			std::string value = header.substr(end + 2);
			header_map_[key] = value;
		}
	}

	return 0;
}
