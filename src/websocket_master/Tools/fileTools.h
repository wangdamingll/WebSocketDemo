#ifndef FILETOOLS_H__
#define FILETOOLS_H__

#define BUFSIZE 1024

#include <string>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>

#include "errorTools.h"

class FileTools{
public:
    //构造函数
    FileTools(const char *filepath);

    FileTools();

    //析构函数
    ~FileTools(void);

    //读文件内容
    bool readNline(std::string &buf, int lines = 1);

    //获取配置信息
    bool getConf(const char *title, std::string &data);

    //获取log文件名
    bool getFilename(const char *dirname, std::string &buf, int num = 1);

    //重置读取位置
    bool resetFd(void);

    //写文件
    int writeFile(std::string &buf);

private:
    std::string     m_filepath; //操作文件
    ErrorTools      m_errtool;  //错误处理
    FILE*           m_fd; //文件操作符
    DIR*            m_dirFd; //文件夹操作符
};

#endif //FILETOOLS_H__
