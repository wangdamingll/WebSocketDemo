#ifndef __WEBSOCKET_RESPOND__
#define __WEBSOCKET_RESPOND__

#include <stdint.h>
#include <arpa/inet.h>
#include "debug_log.h"

#define MSGMAXLEN   2048


class Websocket_Respond {
public:
	Websocket_Respond();
	~Websocket_Respond();

public:
    int  pack_data(const unsigned char* message,size_t msg_len, uint8_t fin , 
                   uint8_t opcode, uint8_t mask, unsigned char** out, size_t* out_len);

private:
	uint8_t fin_;
	uint8_t opcode_;
	uint8_t mask_;
	//uint8_t masking_key_[4];
	//uint64_t payload_length_;
	//char    payload_[MSGMAXLEN];
};

#endif
