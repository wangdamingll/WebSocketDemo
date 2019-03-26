#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <string>
#include <stdint.h>
#include <map>
#include <iconv.h>

typedef std::map<std::string, std::string> URL_PARAMS;

class CHttpclient
{
public:
	enum HttpResult
	{
		EHTTP_SUCESS,
		EHTTP_INVALID_REQUESTURL,
		EHTTP_TCP_INIT_ERROR,
		EHTTP_HOST_CONNECT_ERROR,
		EHTTP_SET_TCP_TIMEOUT_ERROR,
		EHTTP_HTTPHEADER_SEND_ERROR,
		EHTTP_HTTPDATA_RECV_ERROR,
		EHTTP_RESPONSE_PARSE_ERROR,
	};

public:
	CHttpclient();
	~CHttpclient();

	static bool GetHttpRequest(const std::string& requestUrl, std::string& response, int32_t timeout);
	static bool PostHttpRequest(const std::string& requestUrl, std::string& response, int32_t timeout);
    static bool ParseParams(const std::string& astrParams, URL_PARAMS& amapParams, const std::string& astrParamsSep = "&", const std::string& astrParamValueSep = "=");
    static std::string GetParam(const URL_PARAMS& amapParams, const std::string& astrParamName);
    //TODO：以下函数先放这里
    static int stringIconv(const std::string& from, const std::string& to, const char* pSrcWord, char* pDesWord, int nDesSize);
    static uint64_t getCurrentTime();
    static char Hex2Char(unsigned char abyChValue);
    static unsigned char Char2Hex(unsigned char abyChValue);
    static std::string UrlEncode(const std::string& astrData);
    static std::string UrlDecode(const std::string& astrData);

private:
	static HttpResult getHttpRequestEx(const std::string& requestUrl, std::string& response, int32_t timeout);
	static HttpResult postHttpRequestEx(const std::string& requestUrl, std::string& response, int32_t timeout);
	static bool parseHttpParameter(const std::string& requestUtl, std::string& httpHost, std::string& httpPage, std::string& httpParameter);
	static bool getHttpData(const std::string& response, std::string &httpData);
};

#endif
