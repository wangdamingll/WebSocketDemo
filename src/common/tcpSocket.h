#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include <string>
#include <stdint.h>
#include "_type.h"
#include <sys/types.h>
#include <sys/socket.h>

class CTcpSocket
{
public:
	CTcpSocket(const std::string& serverIp, uint16_t serverPort);
	~CTcpSocket();

	bool Init();
	bool Connect(bool timeout = false);
	bool SetRecvTimeout(int timeout);
	int Send(char *buff, int buffLen);
	int Recv(char *buff, int buffLen);
	bool CloseSocket();
	bool Connected();
	bool SetNonBlock();

	SOCKET Socket();

private:
	bool SetSocketOption();

private:
	std::string _serverIp;
	uint16_t _serverPort;
	SOCKET _socket;
	bool _connected;
};

#endif // TCPSOCKET_H
