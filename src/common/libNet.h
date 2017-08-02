#ifndef LIBNET_H
#define LIBNET_H

#include <string>
#ifdef _WIN32
#include <Winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <netdb.h>
#endif // _WIN32

class CLibNet
{
public:
	static std::string GetIpByName(const std::string& host);
};

#endif

