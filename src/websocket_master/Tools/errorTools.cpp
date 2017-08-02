#include "errorTools.h"
//构造函数
ErrorTools::ErrorTools(const char *text):m_text(text)
{
}
ErrorTools::ErrorTools():m_text("Not error data"){}

//析构函数
ErrorTools::~ErrorTools()throw()
{}

const char* ErrorTools::what(void)const throw()
{
    return m_text.c_str();
}

ErrorTools& ErrorTools::operator=(const char *str)throw()
{
    m_text = str;
    return *this;
}
