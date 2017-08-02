#ifndef TIMETOOLS_H__
#define TIMETOOLS_H__
#include <string>

class TimeTools
{
public:
    //获取当前毫秒
    static long long getMsec(void);

    //获取当前时间:年月日时分
    static unsigned long long getCurrTime(void);
};

#endif //TIMETOOLS_H__
