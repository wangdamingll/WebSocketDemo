#include "HttpClient.h"
#include "debugtrace.h"
#include "string_common.h"
#include "libNet.h"
#include "tcpSocket.h"

CHttpclient::CHttpclient()
{

}

CHttpclient::~CHttpclient()
{

}

bool CHttpclient::GetHttpRequest(const std::string& requestUrl, std::string& response, int32_t timeout)
{
	int result = getHttpRequestEx(requestUrl, response, timeout);
	if (result == EHTTP_SUCESS) 
		return true;
	else 
	{
		TRACE(LOG_ERROR, "get http request error, url: "<<requestUrl<<", result: "<<result);
		return false;
	}
}

CHttpclient::HttpResult CHttpclient::getHttpRequestEx(const std::string& requestUrl, std::string& response, int32_t timeout)
{
	TRACE(LOG_TRACE, "requestUrl: "<<requestUrl);
	std::string httpHost, httpPage, httpParameter;
	if (!parseHttpParameter(requestUrl, httpHost, httpPage, httpParameter)) 
	{
		TRACE(LOG_ERROR, "parse requestUrl error, requestUrl: "<<requestUrl);
		return EHTTP_INVALID_REQUESTURL;
	} 
	TRACE(LOG_TRACE, "requestUrl: "<<requestUrl<<", httpHost: "<<httpHost<<", httpPage: "<<httpPage<<", httpParameter: "<<httpParameter);
	
	std::string httpHeader = "GET " + httpPage;
	if (!httpParameter.empty()) 
		httpHeader += "?" + httpParameter;
	httpHeader += " HTTP/1.0\r\n";
	httpHeader += "Accept: */*\r\n";
	httpHeader += "Accept-Language: zh-cn\r\n";
	httpHeader += "User-Agent: Mozilla/4.0\r\n";
	httpHeader += "Host: " + httpHost + "\r\n";
	httpHeader += "Connection: close\r\n";
	httpHeader += "\r\n";
	
	std::string httpIp;
	uint16_t httpPort = 80;
	std::string::size_type portPos = httpHost.find(':');
	if (portPos != std::string::npos) 
	{
		httpIp = httpHost.substr(0, portPos);
		std::string portStr = httpHost.substr(portPos + 1);
		STRING::fromString(portStr, httpPort);
	} 
	httpIp = CLibNet::GetIpByName(httpIp);

	CTcpSocket tcpClient(httpIp, httpPort);
	if (!tcpClient.Init())
	{
		TRACE(LOG_ERROR, "tcp init error");
		return EHTTP_TCP_INIT_ERROR;
	}
	if (!tcpClient.Connect(true)) 
	{
		TRACE(LOG_ERROR, "connect host: "<<httpHost<<" error");
		return EHTTP_HOST_CONNECT_ERROR;
	} 
	if (!tcpClient.SetRecvTimeout(timeout)) 
	{
		TRACE(LOG_ERROR, "set tcp recv timeout error");
		return EHTTP_SET_TCP_TIMEOUT_ERROR;
	} 
	int sendLen = tcpClient.Send((char *)httpHeader.c_str(), httpHeader.length());
	if (sendLen != httpHeader.length()) 
	{
		TRACE(LOG_ERROR, "send httpHeader error, httpHeader length: "<<httpHeader.length()<<", sendLen: "<<sendLen);
		return EHTTP_HTTPHEADER_SEND_ERROR;
	} 
	char recvBuff[20480];
	int recvLen = tcpClient.Recv(recvBuff, 20480);
	tcpClient.CloseSocket();
	if (recvLen <= 0) 
	{
		TRACE(LOG_ERROR, "recv http data error");
		return EHTTP_HTTPDATA_RECV_ERROR;
	} 

	std::string httpData(recvBuff, recvLen);
	TRACE(LOG_TRACE, "httpData: "<<httpData);
	if (getHttpData(httpData, response)) 
	{
		TRACE(LOG_TRACE, "response: "<<response);
		return EHTTP_SUCESS;
	}
	else 
	{
		TRACE(LOG_TRACE, "response: "<<response);
		return EHTTP_RESPONSE_PARSE_ERROR;
	}	
}

bool CHttpclient::parseHttpParameter(const std::string& requestUtl, std::string& httpHost, std::string& httpPage, std::string& httpParameter)
{
	if (requestUtl.empty())
		return false;
	std::string::size_type pos = requestUtl.find("http://");
	if (pos == std::string::npos)
		return false;
	std::string::size_type hostPos = requestUtl.find('/', 7);
	if (hostPos == std::string::npos)
		return false;
	httpHost = requestUtl.substr(7, hostPos - 7);
	std::string::size_type pathPos = requestUtl.find('?');
	if (pathPos == std::string::npos) 
	{
		httpPage = requestUtl.substr(hostPos);
		httpParameter = "";
	} 
	else 
	{
		httpPage = requestUtl.substr(hostPos, pathPos - hostPos);
		httpParameter = requestUtl.substr(pathPos + 1);
	}
	return true;
}

bool CHttpclient::getHttpData(const std::string& response, std::string &httpData)
{
	std::string::size_type posContentLength = response.find("Content-Length:");
	if (posContentLength != std::string::npos) 
	{
		size_t posNum = response.find("\r\n", posContentLength + 1);
		if (posNum == std::string::npos) 
			return false;
		std::string lenStr = response.substr(posContentLength + strlen("Content-Length:"), posNum - posContentLength - strlen("Content-Length:"));
		int32_t len;
		STRING::fromString(lenStr, len);
		size_t endPos = response.find("\r\n\r\n");
		if (endPos == std::string::npos)
			return false;
		if (response.length() - endPos - strlen("\r\n\r\n") >= len) 
		{
			httpData = response.substr(endPos + 4, len);
			return true;
		}
	} 
	else 
	{
		size_t endPos = response.find("\r\n\r\n");
		if (endPos == std::string::npos)
			return false;
		httpData = response.substr(endPos + 4);
		return true;
	}
	return false;
}

bool CHttpclient::PostHttpRequest(const std::string& requestUrl, std::string& response, int32_t timeout)
{
	int result = postHttpRequestEx(requestUrl, response, timeout);
	if (result == EHTTP_SUCESS) 
		return true;
	else 
	{
		TRACE(LOG_ERROR, "post http request error, url: "<<requestUrl<<", result: "<<result);
		return false;
	}
}

CHttpclient::HttpResult CHttpclient::postHttpRequestEx(const std::string& requestUrl, std::string& response, int32_t timeout)
{
	TRACE(LOG_TRACE, "requestUrl: "<<requestUrl);
	std::string httpHost, httpPage, httpParameter;
	if (!parseHttpParameter(requestUrl, httpHost, httpPage, httpParameter)) 
	{
		TRACE(LOG_ERROR, "parse requestUrl error, requestUrl: "<<requestUrl);
		return EHTTP_INVALID_REQUESTURL;
	} 
	TRACE(LOG_TRACE, "requestUrl: "<<requestUrl<<", httpHost: "<<httpHost<<", httpPage: "<<httpPage<<", httpParameter: "<<httpParameter);
	
	std::string httpParameterLen;
	httpParameterLen = STRING::toString(httpParameter.size());
	
	std::string httpHeader = "POST " + httpPage;
	httpHeader += " HTTP/1.0\r\n";
	httpHeader += "Accept: */*\r\n";
	httpHeader += "Accept-Language: zh-cn\r\n";
	httpHeader += "User-Agent: Mozilla/4.0\r\n";
	httpHeader += "Host: " + httpHost + "\r\n";
	httpHeader += "Connection: close\r\n";
	httpHeader += "Content-Length:" + httpParameterLen + "\r\n";
	httpHeader += "\r\n";
	httpHeader += httpParameter;
	
	std::string httpIp;
	uint16_t httpPort = 80;
	std::string::size_type portPos = httpHost.find(':');
	if (portPos != std::string::npos) 
	{
		httpIp = httpHost.substr(0, portPos);
		std::string portStr = httpHost.substr(portPos + 1);
		STRING::fromString(portStr, httpPort);
	} 
	httpIp = CLibNet::GetIpByName(httpIp);

	CTcpSocket tcpClient(httpIp, httpPort);
	if (!tcpClient.Init())
	{
		TRACE(LOG_ERROR, "tcp init error");
		return EHTTP_TCP_INIT_ERROR;
	}
	if (!tcpClient.Connect(true)) 
	{
		TRACE(LOG_ERROR, "connect host: "<<httpHost<<" error");
		return EHTTP_HOST_CONNECT_ERROR;
	} 
	if (!tcpClient.SetRecvTimeout(timeout)) 
	{
		TRACE(LOG_ERROR, "set tcp recv timeout error");
		return EHTTP_SET_TCP_TIMEOUT_ERROR;
	} 
	int sendLen = tcpClient.Send((char *)httpHeader.c_str(), httpHeader.length());
	if (sendLen != httpHeader.length()) 
	{
		TRACE(LOG_ERROR, "send httpHeader error, httpHeader length: "<<httpHeader.length()<<", sendLen: "<<sendLen);
		return EHTTP_HTTPHEADER_SEND_ERROR;
	} 
	char recvBuff[20480];
	int recvLen = tcpClient.Recv(recvBuff, 20480);
	tcpClient.CloseSocket();
	if (recvLen <= 0) 
	{
		TRACE(LOG_ERROR, "recv http data error");
		return EHTTP_HTTPDATA_RECV_ERROR;
	} 

	std::string httpData(recvBuff, recvLen);
	TRACE(LOG_TRACE, "httpData: "<<httpData);
	if (getHttpData(httpData, response)) 
	{
		TRACE(LOG_TRACE, "response: "<<response);
		return EHTTP_SUCESS;
	}
	else 
	{
		TRACE(LOG_TRACE, "response: "<<response);
		return EHTTP_RESPONSE_PARSE_ERROR;
	}	
}

bool CHttpclient::ParseParams(const string& astrParams, URL_PARAMS& amapParams, const string& astrParamsSep, const string& astrParamValueSep)
{
    string lstrParam, lstrParamName, lstrParamValue;
    string::size_type lStartPos = 0, lValuePos = 0;
    string::size_type lPos = astrParams.find(astrParamsSep);
    while (string::npos != lPos)
    {
        lstrParam = astrParams.substr(lStartPos, lPos - lStartPos);
        lValuePos = lstrParam.find(astrParamValueSep);
        if (string::npos != lValuePos)
        {
            lstrParamName = lstrParam.substr(0, lValuePos);
            lstrParamValue = lstrParam.substr(lValuePos+astrParamValueSep.size());
        }
        else
        {
            lstrParamName = lstrParam;
            lstrParamValue = "";
        }
        amapParams[lstrParamName] = lstrParamValue;
        lStartPos = lPos + astrParamsSep.size();
        lPos = astrParams.find(astrParamsSep, lStartPos);
    }
    if (lStartPos < astrParams.size())
    {
        lstrParam = astrParams.substr(lStartPos);
        lValuePos = lstrParam.find('=');
        if (string::npos != lValuePos)
        {
            lstrParamName = lstrParam.substr(0, lValuePos);
            lstrParamValue = lstrParam.substr(lValuePos+astrParamValueSep.size());
        }
        else
        {
            lstrParamName = lstrParam;
            lstrParamValue = "";
        }
        amapParams[lstrParamName] = lstrParamValue;
    }
    return !amapParams.empty();
}

string CHttpclient::GetParam(const URL_PARAMS& amapParams, const string& astrParamName)
{
    URL_PARAMS::const_iterator lPos = amapParams.find(astrParamName);
    if (amapParams.end()!= lPos)
    {
        return lPos->second;
    }
    return "";
}

int CHttpclient::stringIconv(const string& from, const string& to, const char* pSrcWord, char* pDesWord, int nDesSize)
{
    size_t result = 0;
    size_t inSize = strlen(pSrcWord);
    size_t outSize = nDesSize;

    iconv_t env = iconv_open(to.c_str(), from.c_str());
    if (env == (iconv_t)-1) {
        TRACE(LOG_ERROR, "iconv_open "<<from<<"->"<<to<<"error: "<<errno);
        return -1;
    }
    result = iconv(env, (char**)&pSrcWord, &inSize, (char**)&pDesWord, &outSize);
    if (result == (size_t)-1) {
        TRACE(LOG_ERROR, "iconv "<<from<<"->"<<to<<" error: "<<errno);
        iconv_close(env);
        return -1;
    }
    iconv_close(env);

    return (int)result;
}

uint64_t CHttpclient::getCurrentTime()
{
    struct timeb loTimeb;
    memset(&loTimeb, 0, sizeof(timeb));
    ftime(&loTimeb);
    return ((uint64_t)loTimeb.time * 1000) + loTimeb.millitm;
}

string CHttpclient::UrlEncode(const string& astrData)
{
    string lstrData;
    char ch = 0;
    const char* pszSrc = astrData.c_str();
    for (int i = 0; i < (int)astrData.size(); i++) {
        ch = pszSrc[i];
        if ((ch >= '0' && ch <='9')
            || (ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')) {
            lstrData += ch;
        }
        else if (' ' == ch) {
            lstrData += "+";
        }
        else {
            lstrData += "%";
            lstrData += Hex2Char((ch>>4)&0x0F);
            lstrData += Hex2Char(ch&0x0F);

        }
    }
    return lstrData;
}

string CHttpclient::UrlDecode(const string& astrData)
{
    std::string lstrData = "";
    for (int i = 0; i < (int)astrData.size(); i++) {
        if ((astrData.at(i) >= '0' && astrData.at(i) <='9')
            || (astrData.at(i) >= 'a' && astrData.at(i) <= 'z')
            || (astrData.at(i) >= 'A' && astrData.at(i) <= 'Z')) {
            lstrData += astrData.at(i);
        }
        else if ('+' == astrData.at(i)) {
            lstrData += " ";
        }
        else if ('%' == astrData.at(i)) {
            if (i + 2 >(int)astrData.size()-1)
                break;
            i++;
            char ch = Char2Hex(astrData.at(i));
            ch<<=4;
            i++;
            ch += Char2Hex(astrData.at(i));
            lstrData += ch;
        }
    }
    return lstrData;
}

char CHttpclient::Hex2Char(unsigned char abyChValue)
{
    if (abyChValue < 10) {
        return '0' + abyChValue;
    }
    else {
        return 'A' + abyChValue - 10;
    }
}

unsigned char CHttpclient::Char2Hex(unsigned char abyChValue)
{
    if (abyChValue < 'A') {
        return abyChValue - '0';
    }
    else {
        return abyChValue - 'A' + 10;
    }
}
