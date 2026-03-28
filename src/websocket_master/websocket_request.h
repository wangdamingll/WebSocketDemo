#ifndef __WEBSOCKET_REQUEST__
#define __WEBSOCKET_REQUEST__

/* 解析客户端→服务端的 WebSocket 帧；try_consume_frame 从 vector 前端取完整一帧 */

#include <stdint.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include "debug_log.h"

using namespace std;

// 单帧 payload 上限，防止恶意大包占满内存
#define WS_MAX_PAYLOAD_BYTES (1024 * 1024)

enum Websocket_ParseResult {
	WS_PARSE_NEED_MORE = 0,
	WS_PARSE_OK        = 1,
	WS_PARSE_ERROR     = -1,
};

class Websocket_Request {
public:
	Websocket_Request();
	~Websocket_Request();

	// 从 read_buf 前端尝试解析一帧；成功则消费该帧并填充字段
	int try_consume_frame(std::vector<char> &buf);

	void print();
	void reset();

	std::string get_msg();
	uint64_t    get_msg_length();
	uint8_t     get_msg_opcode_();

private:
	uint8_t fin_;
	uint8_t opcode_;
	uint8_t mask_;
	uint8_t masking_key_[4];
	uint64_t payload_length_;
	std::string payload_;
};

#endif
