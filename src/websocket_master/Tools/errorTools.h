#ifndef ERRORTOOLS_H__
#define ERRORTOOLS_H__
#include <string>
#include <iostream>

class ErrorTools:public std::exception{
public:
    //构造函数
    ErrorTools(const char *text);
    ErrorTools();
    ~ErrorTools()throw();

    const char* what(void)const throw();

    ErrorTools& operator=(const char *)throw();
private:
    std::string     m_text; //错误信息
};
#endif //ERRORTOOLS_H__
