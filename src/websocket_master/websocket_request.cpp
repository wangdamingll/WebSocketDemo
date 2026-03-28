#include "websocket_request.h"
#include <string.h>

Websocket_Request::Websocket_Request():
		fin_(),
		opcode_(),
		mask_(),
		masking_key_(),
		payload_length_(),
		payload_()
{
}

Websocket_Request::~Websocket_Request(){}

int Websocket_Request::try_consume_frame(std::vector<char> &buf){
	const size_t n = buf.size();
	if (n < 2)
		return WS_PARSE_NEED_MORE;

	const unsigned char *p = reinterpret_cast<const unsigned char *>(buf.data());
	fin_ = p[0] >> 7;
	opcode_ = p[0] & 0x0f;
	mask_ = p[1] >> 7;
	uint64_t len7 = p[1] & 0x7f;

	size_t header_len = 2;
	uint64_t payload_len = 0;

	if (len7 < 126) {
		payload_len = len7;
	} else if (len7 == 126) {
		if (n < 4)
			return WS_PARSE_NEED_MORE;
		uint16_t ext = 0;
		memcpy(&ext, p + 2, 2);
		payload_len = ntohs(ext);
		header_len = 4;
	} else {
		if (n < 10)
			return WS_PARSE_NEED_MORE;
		uint64_t ext = 0;
		for (int i = 0; i < 8; ++i)
			ext = (ext << 8) | static_cast<uint64_t>(p[2 + i]);
		if (ext >> 63)
			return WS_PARSE_ERROR;
		payload_len = ext;
		header_len = 10;
	}

	if (mask_) {
		if (n < header_len + 4)
			return WS_PARSE_NEED_MORE;
		for (int i = 0; i < 4; ++i)
			masking_key_[i] = p[header_len + i];
		header_len += 4;
	} else {
		memset(masking_key_, 0, sizeof(masking_key_));
	}

	if (payload_len > WS_MAX_PAYLOAD_BYTES)
		return WS_PARSE_ERROR;

	const size_t total = header_len + static_cast<size_t>(payload_len);
	if (n < total)
		return WS_PARSE_NEED_MORE;

	const unsigned char *payload_src = p + header_len;
	payload_.assign(payload_len, '\0');
	if (mask_) {
		for (uint64_t i = 0; i < payload_len; ++i)
			payload_[static_cast<size_t>(i)] =
				static_cast<char>(payload_src[i] ^ masking_key_[i % 4]);
	} else {
		memcpy(&payload_[0], payload_src, static_cast<size_t>(payload_len));
	}

	payload_length_ = payload_len;
	buf.erase(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(total));
	return WS_PARSE_OK;
}

void Websocket_Request::print(){
	DEBUG_LOG("WEBSOCKET PROTOCOL\n"
				"FIN: %d\n"
				"OPCODE: %d\n"
				"MASK: %d\n"
				"PAYLOADLEN: %llu",
				fin_, opcode_, mask_,
				static_cast<unsigned long long>(payload_length_));
}

void Websocket_Request::reset(){
	fin_ = 0;
	opcode_ = 0;
	mask_ = 0;
	memset(masking_key_, 0, sizeof(masking_key_));
	payload_length_ = 0;
	payload_.clear();
}

std::string Websocket_Request::get_msg(){
	return payload_;
}

uint64_t Websocket_Request::get_msg_length(){
	return payload_length_;
}

uint8_t Websocket_Request::get_msg_opcode_(){
	return opcode_;
}
