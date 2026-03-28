#ifndef __WEBSOCKET_HANDLER__
#define __WEBSOCKET_HANDLER__

/* 单 fd 的 WebSocket 状态机：读缓冲拼帧、写缓冲、握手与业务帧处理 */

#include <arpa/inet.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "base64.h"
#include "sha1.h"
#include "debug_log.h"
#include "websocket_request.h"
#include "websocket_respond.h"

#define MAGIC_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

enum WEBSOCKET_STATUS {
	WEBSOCKET_UNCONNECT         = 0,
	WEBSOCKET_HANDSHARKED       = 1,
};

enum WEBSOCKET_FIN_STATUS {
     WEBSOCKET_FIN_MSG_END      =1,
     WEBSOCKET_FIN_MSG_NOT_END  =0,
};

enum WEBSOCKET_OPCODE_TYPE {
     WEBSOCKET_APPEND_DATA      =0X0,
     WEBSOCKET_TEXT_DATA        =0X1,
     WEBSOCKET_BINARY_DATA      =0X2,
     WEBSOCKET_CONNECT_CLOSE    =0X8,
     WEBSOCKET_PING_PACK        =0X9,
     WEBSOCKET_PANG_PACK        =0XA,
};

enum WEBSOCKET_MASK_STATUS {
     WEBSOCKET_NEED_MASK        =1,
     WEBSOCKET_NEED_NOT_MASK    =0,
};

typedef std::map<std::string, std::string>  HEADER_MAP;

class Websocket_Handler{
public:
	Websocket_Handler(int fd);
	~Websocket_Handler();

	bool append_read(const void *data, size_t len);
	void drive_io();
	void flush_writes();
	void append_outgoing(const void *data, size_t len, bool update_epoll = true);
	bool write_pending() const;

private:
	void    parse_str(char *request);
	int     fetch_http_info(const std::string &raw_headers);
	void    handle_one_frame();
	void    queue_send(const void *data, size_t len);

private:
	std::vector<char>       read_buf_;
	std::string             write_buf_;
	WEBSOCKET_STATUS        status_;
	HEADER_MAP              header_map_;
	int                     fd_;
	Websocket_Request *     request_;
	Websocket_Respond *     respond_;
};

#endif
