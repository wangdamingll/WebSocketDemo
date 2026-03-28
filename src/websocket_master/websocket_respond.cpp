/*
 * websocket_respond.cpp — 服务端出站帧编码（RFC 6455）
 * 支持 7bit / 16bit / 64bit payload 长度；mask 位按协议对服务端应为 0。
 */

#include "websocket_respond.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

Websocket_Respond::Websocket_Respond(): fin_(),opcode_(), mask_(){}
Websocket_Respond::~Websocket_Respond() { }


int  Websocket_Respond::pack_data(const unsigned char* message,size_t msg_len, uint8_t fin ,
        uint8_t opcode, uint8_t mask, unsigned char** out, size_t* out_len)
{
	if(message == NULL || out_len== NULL ){
		return -1;
	}

	size_t hdr = 2;
	if (msg_len < 126) {
		/* hdr stays 2 */
	} else if (msg_len <= 0xFFFFu) {
		hdr = 4;
	} else {
		hdr = 10;
	}

	const size_t total = hdr + msg_len;
	unsigned char* buf = (unsigned char*)malloc(total);
	if (!buf)
		return -1;

	buf[0] = (unsigned char)((fin << 7) | (opcode & 0x0F));
	buf[1] = (unsigned char)(mask << 7);

	if (msg_len < 126) {
		buf[1] |= (unsigned char)msg_len;
		memcpy(buf + 2, message, msg_len);
	} else if (msg_len <= 0xFFFFu) {
		buf[1] |= 126;
		uint16_t be = htons((uint16_t)msg_len);
		memcpy(buf + 2, &be, sizeof(be));
		memcpy(buf + 4, message, msg_len);
	} else {
		buf[1] |= 127;
		uint64_t v = (uint64_t)msg_len;
		for (int i = 0; i < 8; ++i)
			buf[2 + i] = (unsigned char)((v >> (56 - i * 8)) & 0xFF);
		memcpy(buf + 10, message, msg_len);
	}

	*out_len = total;
	*out = buf;
	return 0;
}
