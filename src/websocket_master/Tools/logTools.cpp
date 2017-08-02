#include "logTools.h"

using namespace std;

LogTools::LogTools(const string &api_name, const string &conf_path):
    m_apiName(api_name),m_confPath(conf_path)
{
    std::string level_buf, logPath;
    FileTools f_tool(m_confPath.c_str()); //打开配置文件

    if(!f_tool.getConf("logLevel", level_buf)) //查找日志级别
    {
        m_erroTool = "An error occurred while get the log level";
        throw m_erroTool;
    }
    m_level = atoi(level_buf.c_str());

    if(!f_tool.getConf("logPath", m_path)) //查找日志路径
    {
        m_path.clear();
    }
}

//创建日志
int LogTools::createLog(void)
{
    if(m_path.empty())
    {//没有设置路径，使用当前路径
        //生成日志类
        if(!initlog(m_apiName, m_level))
        {
            return -2;
        }
    }
    else
    {//设置了日志路径，使用路径
        //生成日志类
        if(!initlog(m_apiName, m_level, m_path))
        {
            return -3;
        }
    }

    //设置日志生成时间
    m_uiLastTime = TimeTools::getCurrTime();
    return 0;
}

//重建日志文件
void LogTools::rebuild(void)
{
    unsigned long long curr = TimeTools::getCurrTime();
    if((m_uiLastTime - curr) < 1) //无需生成日志
    {
        return ;
    }
    delete goDebugTrace;
    goDebugTrace = NULL;
    int proof = createLog();
    TRACE(1, "create log successful");
    if(proof < 0) //重建日志错误
    {
        TRACE(LOG_ERROR, "An error occurred while rebuild the log file! " << proof);
        return ;
    }

    m_uiLastTime = curr; //重置生成时间
}



