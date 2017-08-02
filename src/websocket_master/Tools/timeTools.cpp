#include "timeTools.h"
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <sstream>
#include <iostream>

using namespace std;

//获取当前毫秒
long long TimeTools::getMsec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000 + tv.tv_usec / 100;
}

//获取当前日期:年月日
unsigned long long TimeTools::getCurrTime(void)
{
    time_t ti = time(NULL);
    struct tm *local = localtime(&ti);

    ostringstream buf;
    buf << local->tm_year + 1900
        << local->tm_mon + 1
        << local->tm_mday;

    return atoll(buf.str().c_str());
}








