#include "tcpSocket.h"
#include "debugtrace.h"

CTcpSocket::CTcpSocket(const std::string& serverIp, uint16_t serverPort)
	: _serverIp(serverIp), _serverPort(serverPort), _socket(INVALID_SOCKET), _connected(false)
{
}

CTcpSocket::~CTcpSocket()
{
	CloseSocket();
}

bool CTcpSocket::Init()
{
    _socket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (_socket == INVALID_SOCKET) 
	{
		TRACE(LOG_ERROR, "create socket error: "<<errno<<"["<<strerror(errno)<<"]");
		return false;
	} 
	
	return SetSocketOption();
}

bool CTcpSocket::Connect(bool timeout /* = false */)
{
	ASSERT(_socket != INVALID_SOCKET);
	if (timeout) 
	{
		struct timeval timeset;
		timeset.tv_sec = 1;
		timeset.tv_usec = 0;
		if (::setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeset, sizeof(timeset)) != 0) 
		{
			TRACE(LOG_ERROR, "setsockopt error: "<<errno<<"["<<strerror(errno)<<"]");
			return false;
		} 
	} 

	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(_serverIp.c_str());
	serverAddr.sin_port = htons(_serverPort);
	if (::connect(_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) 
	{
		if (errno == EINPROGRESS) 
		{
			TRACE(LOG_ERROR, "server "<<_serverIp<<":"<<_serverPort<<" is connecting");
			return false;
		} 
		TRACE(LOG_ERROR, "error "<<errno<<"["<<strerror(errno)<<"] occur when connecting server "<<_serverIp<<":"<<_serverPort);
		return false;
	} 
	_connected = true;
	return true;
}

bool CTcpSocket::SetRecvTimeout(int timeout)
{
	struct timeval timeset;
	timeset.tv_sec = timeout;
	timeset.tv_usec = 0;
	if (::setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeset, sizeof(timeset)) != 0) 
	{
		TRACE(LOG_ERROR, "setsockopt error: "<<errno<<"["<<strerror(errno)<<"]");
		return false;
	} 
	return true;
}

int CTcpSocket::Send(char *buff, int buffLen)
{
	int sendLen = 0;
	int len = 0;
	do 
	{
		len = ::send(_socket, buff + sendLen, buffLen - sendLen, 0);
		if (len > 0)
			sendLen += len;
	} while (len > 0 && sendLen < buffLen);
	
	TRACE(LOG_INFO, "send data size: "<<sendLen);
	if (len < 0) 
	{
		TRACE(LOG_ERROR, "send data error: "<<errno<<"["<<strerror(errno)<<"]");
		if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;
	} 
	return sendLen;
}

int CTcpSocket::Recv(char *buff, int buffLen)
{
	int len = ::recv(_socket, buff, buffLen, 0);

	if (len > 0)
	{
		TRACE(LOG_INFO, "recv data size: "<<len);
	}
	else if (len == 0) 
	{
		TRACE(LOG_INFO, "peer maybe has been closed gracefully");
	} 
	else if (len < 0) 
	{
		if (errno == EAGAIN || errno == EINTR)
			return 0;
        TRACE(LOG_ERROR, "recv data error: "<<errno<<"["<<strerror(errno)<<"]");
	} 
	return len;
}

bool CTcpSocket::CloseSocket()
{
	if (_socket != INVALID_SOCKET) 
	{
#ifdef _WIN32
		::shutdown(_socket, SD_BOTH);
		::closesocket(_socket);
#else
		::shutdown(_socket, SHUT_RDWR);
		::close(_socket);
#endif // _WIN32
		_socket = INVALID_SOCKET;
		_connected = false;
	} 
	return true;
}

bool CTcpSocket::Connected()
{
	return _connected;
}

bool CTcpSocket::SetSocketOption()
{
	int32_t nFlags = 1;
	struct linger ling = { 0, 0 };

	setsockopt(_socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&nFlags, sizeof(nFlags));
	setsockopt(_socket, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));

	int sendBuff = 20480;
	if (::setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, (char *)&sendBuff, sizeof(sendBuff)) != 0)
	{
		TRACE(LOG_ERROR, "set sendBuff error: " << errno << "[" << strerror(errno) << "]");
		return false;
	}
	int recvBuff = 20480;
	if (::setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (char *)&recvBuff, sizeof(recvBuff)) != 0)
	{
		TRACE(LOG_ERROR, "set recvBuff error: " << errno << "[" << strerror(errno) << "]");
		return false;
	}
	return true;
}

bool CTcpSocket::SetNonBlock()
{
#ifdef _WIN32
	unsigned long ioctl_opt;
	ioctlsocket(_socket, FIONBIO, &ioctl_opt);
#else
	int32_t flags = 1;
	if (ioctl(_socket, FIONBIO, &flags))
	{
		return false;
	}
	if (-1 == (flags = fcntl(_socket, F_GETFL)))
	{
		return false;
	}
	if (flags & O_NONBLOCK)
	{
		return true;
	}
	if (-1 == fcntl(_socket, F_SETFL, flags | O_NONBLOCK))
	{
		return false;
	}
#endif
	return true;
}

SOCKET CTcpSocket::Socket()
{
	return _socket;
}
