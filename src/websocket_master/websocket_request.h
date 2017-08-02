#ifndef __WEBSOCKET_REQUEST__
#define __WEBSOCKET_REQUEST__

#include <stdint.h>
#include <arpa/inet.h>
#include <string>
#include "debug_log.h"

using namespace std;

class Websocket_Request {
public:
	Websocket_Request();
	~Websocket_Request();

public:
	int fetch_websocket_info(char *msg);
	void print();
	void reset();

    std::string get_msg();
    uint64_t    get_msg_length();
    uint8_t     get_msg_opcode_();

private:
	int fetch_fin(char *msg, int &pos);
	int fetch_opcode(char *msg, int &pos);
	int fetch_mask(char *msg, int &pos);
	int fetch_masking_key(char *msg, int &pos);
	int fetch_payload_length(char *msg, int &pos);
	int fetch_payload(char *msg, int &pos);
private:
	uint8_t fin_;
	uint8_t opcode_;
	uint8_t mask_;
	uint8_t masking_key_[4];
	uint64_t payload_length_;
	char payload_[2048];
};

#endif
