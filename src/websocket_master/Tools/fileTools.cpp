#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "fileTools.h"
#include "errorTools.h"

//构造函数
FileTools::FileTools(const char *filepath)
    :m_filepath(filepath)
{
    m_dirFd = NULL;
    //以读模式打开文件
    m_fd = fopen(m_filepath.c_str(), "r");
    if(m_fd == NULL)
    {
        m_errtool = strerror(errno); //获取系统异常信息
        throw(m_errtool); //抛出异常
    }
}

FileTools::FileTools():m_filepath(""),m_errtool("Not error")
{
    m_fd = NULL;
    m_dirFd = NULL;
}

//析构函数
FileTools::~FileTools(void)
{
    //关闭文件
    if(m_fd != NULL)
    {
        fclose(m_fd);
    }
}

//读文件
//参数：lines:读取多少行, 默认读取1行
bool FileTools::readNline(std::string &buf, int lines/*=1*/)
{
    if(m_fd == NULL)
    {
        m_errtool = "m_fd == NULL";
        throw(m_errtool);
    }
    char read_buf[BUFSIZE] = {0};
    buf.clear(); //清空buf
    while(fgets(read_buf,sizeof(read_buf), (m_fd))!= NULL)
    {
        buf += read_buf;
        if(!--lines)
            break;
    }

    if(lines > 0)
    {
        return false;
    }

    return true;
}

//写文件 暂时不实现
int FileTools::writeFile(std::string &buf)
{
    return 0;
}

//获取配置信息
bool FileTools::getConf(const char *title, std::string &data)
{
    std::string buf;
    char *str = NULL;
    char line[BUFSIZE] = {0}, ret[BUFSIZE] = {0};
    resetFd();
    while(readNline(buf))
    {
        sprintf(line, "%s", buf.c_str());
        if((str = strstr(line, title)) != NULL)
        {
            line[strlen(line)-2] = '\0';
            sprintf(ret, "%s", (str + strlen(title) + 1));
            data = (str + strlen(title) + 1);
            return true;
        }
    }

    return false;
}

//重置读取位置
bool FileTools::resetFd(void)
{
    if(fseek(m_fd, 0, SEEK_SET) == -1) 
    {
        return false;
    }
    return true;
}

//获取文件名
bool FileTools::getFilename(const char *dirname, std::string &buf, int num/*=1*/)
{
    if((m_dirFd = opendir(dirname)) == NULL)
    {
        m_errtool = strerror(errno); //获取错误信息
        throw(m_errtool); //抛出异常
    }
    struct dirent *p_dir = NULL;
    while((p_dir = readdir(m_dirFd)) != NULL) //读取目录下的文件名
    {
        char tmp[200] = {0};
        if(strcmp(dirname+strlen(dirname)-1, "/") == 0)
        {
            sprintf(tmp, "%s%s",dirname, p_dir->d_name);
        }
        else
        {
            sprintf(tmp, "%s/%s", dirname, p_dir->d_name);
        }
        tmp[strlen(tmp)] = '\0';
        buf = tmp; //获取文件名
        if(!--num)
            break;
    }

    if(num > 0)
        return false;
    else
        return true;
}





