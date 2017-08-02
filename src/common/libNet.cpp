#include "libNet.h"
#include "debugtrace.h"

std::string CLibNet::GetIpByName(const std::string& host)
{
    hostent *pHostInfo = NULL;
#ifdef _WIN32
    WSADATA wsaData;
    int iResult;
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        TRACE(LOG_ERROR, "WSAStartup failed: "<<iResult);
        return "";
    }

    pHostInfo = gethostbyname(host.c_str());
#else
    hostent hostinfo;
    char buff[8192] = { 0 };
    int ret = 0;
    if (0 != gethostbyname_r(host.c_str(), &hostinfo, buff, 8192, &pHostInfo, &ret))
    {
        TRACE(LOG_ERROR, "gethostbyname_r error, host: "<<host<<", ret: "<<ret);
        return "";
    }
#endif

    if (NULL == pHostInfo)
    {
        TRACE(LOG_ERROR, "pHostInfo is NULL");
        return "";
    }
    return inet_ntoa(*(in_addr*)*(pHostInfo->h_addr_list));
}
