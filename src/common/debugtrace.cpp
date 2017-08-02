/***********************************************************
版权声明：  Copyright (C) 2002 - LongMaster Co.Ltd
文 件 名：  DebugTrace.cpp
创 建 人：  张宏涛
创建日期：  2003-09-15
说    明：  用来打印调试数据,包括各种通用数据类型
版 本 号：  0.0001

张亚伟	2006-11-13 修改
目的是适应跨平台应用
***********************************************************/ 
#include <stdio.h>
#include <stdint.h>
#include "_type.h"
#include "debugtrace.h"
//#include "FileStream.h" 

char goLogMark[] = "SERVER_LOG_BEGIN";
CDebugTrace	*goDebugTrace = NULL;       //打印日志的DebugTrace对象

int64_t		gi64TraceUserId;	//追踪的用户ID

//***************************************************************************** 
//  函数原型：  CDebugTrace(unsigned asTraceOptions) 
//  参数：      unsigned asTraceOptions (日志打印选项,默认打印时间且写入到文件) 
//  返回值：     
//  用法：      构造函数 
//***************************************************************************** 
// $_CODE_CHANGE 2005-09-09 fyf修改：初始化缓冲区长度 
CDebugTrace::CDebugTrace(unsigned asTraceOptions)
{
	mlDataLen = 0;  
	mnLogLevel = 4;

	memset(mszLogFileName,0,512);
	memset(mszLogFileNamePre, 0, 450);

	//清空临时缓冲区 
	memset(mszPrintBuff, 0, sizeof(mszPrintBuff));

    muTraceOptions = asTraceOptions;
	//默认日志文件大小为1G 
	muiLogFileSize = 100*1024*1024;
	lfpTraceFile_ = NULL;

#ifndef LOG_OUTPUT_TO_FILE_DIRECT
	//保证缓冲区的尺寸 
	ASSERT(LOG_BUFFER_LEN > DEF_MAX_BUFF_LEN*5);

	//在缓冲区的前面加标记 
	strncpy(macMark, goLogMark, sizeof(macMark));

	//清空缓冲区 
	mlBufDataLen = 0 ;
#endif 
}

//***************************************************************************** 
//  函数原型：  ~CDebugTrace() 
//  参数：        
//  返回值：     
//  用法：      析够函数 
//***************************************************************************** 
// $_CODE_CHANGE 2005-09-09 fyf修改：在析构的时候清除所有的缓冲区 
CDebugTrace::~CDebugTrace()
{
#ifndef LOG_OUTPUT_TO_FILE_DIRECT
	Flush();
#endif //LOG_OUTPUT_TO_FILE_DIRECT 

	if (lfpTraceFile_ != NULL)
		fclose(lfpTraceFile_); 
}

//打印单个字符 
CDebugTrace& CDebugTrace::operator << (unsigned char acCharVal)
{
	if (mlDataLen < DEF_MAX_BUFF_LEN - 2)
	{
		char * lpWritePtr = mszPrintBuff + mlDataLen;		
		mlDataLen += sprintf(lpWritePtr,"%d",acCharVal);
	}
	return *this;
}

//打印bool值 
CDebugTrace& CDebugTrace::operator << (bool abBoolVal)
{
	if (mlDataLen < DEF_MAX_BUFF_LEN - 6)
	{
		char * lpWritePtr = mszPrintBuff + mlDataLen;
		if (abBoolVal)
		{   
			mlDataLen += sprintf(lpWritePtr,"%s","true");
		}
		else
		{
			mlDataLen += sprintf(lpWritePtr,"%s","false");
		}
	}
	return *this;
}

#ifndef x86_64
//打印64位整数(int64_t) 
CDebugTrace& CDebugTrace::operator << (int64_t aiint64Val)
{
	if (mlDataLen < DEF_MAX_BUFF_LEN - 20) //max:18446744073709551615 
	{
		char *lpWritePtr = mszPrintBuff + mlDataLen;
		mlDataLen += sprintf(lpWritePtr,"%lld",aiint64Val);
	}
	return *this;
}
#endif

//打印字符串值 
CDebugTrace& CDebugTrace::operator << (const char *apStrVal)
{	
	char * lpWritePtr = mszPrintBuff + mlDataLen;
	if (apStrVal == 0)
	{       
		if (mlDataLen < (int)(DEF_MAX_BUFF_LEN - strlen("NULL")))
			mlDataLen += sprintf(lpWritePtr,"%s","NULL");
	}
	else
	{
		if (mlDataLen < (int)(DEF_MAX_BUFF_LEN - strlen(apStrVal)))
			mlDataLen += sprintf(lpWritePtr,"%s",apStrVal);
	}   	
	return *this;
}

//inline _CRTIMP ostream& __cdecl endl(ostream& _outs) { return _outs << '\n' << flush; }
CDebugTrace& CDebugTrace::endl(CDebugTrace &aoDebugTrace)
{
	//若要求输出到控制台,则把日志信息在控制台也打印一下 
	if (aoDebugTrace.muTraceOptions & CDebugTrace::PrintToConsole) {
		printf("%s",aoDebugTrace.mszPrintBuff);
	}

	//若要求写文件且设置了日志文件名,则把日志信息写入文件中 
	if ((aoDebugTrace.muTraceOptions & CDebugTrace::AppendToFile) && (strlen(aoDebugTrace.mszLogFileName) > 1))
	{
		if (lfpTraceFile_ == NULL)
		{
			lfpTraceFile_  = fopen(aoDebugTrace.mszLogFileName,"a");  
			if (lfpTraceFile_ != NULL)
			{
				fprintf(lfpTraceFile_,"%s",aoDebugTrace.mszPrintBuff);
			}
		}
	}
	aoDebugTrace.mlDataLen = 0;
	moCriticalSection.Leave(); //退出临界区
	return aoDebugTrace;	
}
//*****************************************************************************
//  函数原型：  TraceFormat(const char * pFmt,...)
//  函数说明:   类似printf函数的格式打印一串日志
//  参数：      const char * pFmt,...(可变长度参数,如:"姓名:%s,年龄:%d","zht",26)
//  返回值：    
//  用法：      TraceFormat("姓名:%s,年龄:%d","zht",26)
//***************************************************************************** 
void CDebugTrace::TraceFormat(const char * pFmt,...)
{
	va_list argptr;
	va_start(argptr, pFmt);

	mlDataLen += vsnprintf(mszPrintBuff + mlDataLen, 
		DEF_MAX_BUFF_LEN - mlDataLen,
		pFmt , argptr);

	va_end(argptr);

	//调用EndTrace结束打印 
	EndTrace();
}

////////////////////////////////////////////////
//以下是静态函数

//设置TRACE等级(0级最高,1其次,依次类推,小于该等级的日志不打印) 
void CDebugTrace::SetTraceLevel(int aiTraceLevel)
{
	mnLogLevel = aiTraceLevel;
}

void CDebugTrace::SetLogFileSize(unsigned int aiSize)
{
	muiLogFileSize = aiSize;
}


//设置输出日志文件名
// $_CODE_CHANGE 2005-09-09 fyf修改：更新用户的文件名的时候先清空一下 
void CDebugTrace::SetLogFileName(char *aszLogFile)
{

	CAutoLock lock(moCriticalSection);

	if (lfpTraceFile_ != NULL)
	{
		fclose(lfpTraceFile_);
		lfpTraceFile_ = NULL;
	}

#ifndef LOG_OUTPUT_TO_FILE_DIRECT
	Flush();
#endif //LOG_OUTPUT_TO_FILE_DIRECT 
	if (aszLogFile != NULL)
	{
		ASSERT(strlen(aszLogFile) <= 512);
		strcpy(mszLogFileNamePre,aszLogFile);
	}
	else
	{
		strcpy(mszLogFileNamePre,"");
	}

	char lszFileDate[50];
	memset(lszFileDate, 0, 50);
	time_t loSystemTime;
	time(&loSystemTime);
	struct tm* lptm = localtime(&loSystemTime);
	sprintf(lszFileDate, "-%4d%02d%02d%02d%02d.log", 
		1900+lptm->tm_year,1+lptm->tm_mon,lptm->tm_mday, 
		lptm->tm_hour, lptm->tm_min);

	memcpy(mszLogFileName, mszLogFileNamePre, strlen(mszLogFileNamePre));
	memcpy(mszLogFileName+strlen(mszLogFileNamePre), lszFileDate, strlen(lszFileDate));
}


// 设置TRACE选项 .这个函数可以 OR 各选项 
void CDebugTrace::SetTraceOptions(unsigned options /** New level for trace */ )
{
	muTraceOptions = options;
}

//取得TRACE选项 
unsigned CDebugTrace::GetTraceOptions(void)
{
	return muTraceOptions;
}

//判断给定级别是否可以打印 
bool CDebugTrace::CanTrace(int aiLogLevel)
{
	return (aiLogLevel <= mnLogLevel) && (aiLogLevel);
}

//*****************************************************************************
//  函数原型：  BeginTrace(int aiLogLevel,char *apSrcFile,int aiSrcLine)
//  函数说明:   开始打印一个日志
//  参数：      int aiLogLevel(日志的级别),char *apSrcFile(源文件名),
//              int aiSrcLine(源行数)
//  返回值：    返回类对象自身
//  用法：      BeginTrace(3,__FILE__,__LINE__)
//***************************************************************************** 
CDebugTrace& CDebugTrace::BeginTrace(int aiLogLevel,char *apSrcFile,int aiSrcLine)
{      
	mlDataLen = 0;  //已打印的数据长度清0 
	memset(mszPrintBuff, 0, (DEF_MAX_BUFF_LEN+1));
	miLogLevel = aiLogLevel;
		
	ftime(&moSystemTime);

	struct tm* lptm = localtime(&moSystemTime.time);

	//如果要求输出时间,则在日志中输出日志产生的时间(分:秒:毫秒) 
	if (muTraceOptions & Timestamp) 
	{
		char lszTraceDataBuff[20];
		sprintf(lszTraceDataBuff,"%02d:%02d:%02d:%03d",\
			lptm->tm_hour, lptm->tm_min, lptm->tm_sec, moSystemTime.millitm);

		*this << lszTraceDataBuff<<' ';
	}
	//如果要求输出日志级别,则在日志中输出日志级别 
	if (muTraceOptions & LogLevel)
	{
		*this << aiLogLevel << ' ';
	}
	//如果要求输出源文件名和行号,则在日志中输出源文件名和行号 
	if ( muTraceOptions & FileAndLine ) 
	{
		*this << apSrcFile << "(" << aiSrcLine << ") ";
	}
	//返回对象 
	return *this;
}


//*****************************************************************************
//  函数原型：  EndTrace(CDebugTrace &aoDebugTrace)
//  函数说明:   结束打印一个日志
//  参数：      CDebugTrace &aoDebugTrace(CDebugTrace 对象引用)
//  返回值：    
//  用法：      
//*****************************************************************************
// $_CODE_CHANGE 2005-09-09 fyf修改：输出到缓冲区中 
void CDebugTrace::EndTrace()       //结束打印
{
	try
	{

		//若要求输出到控制台,则把日志信息在控制台也打印一下 
		if (muTraceOptions & PrintToConsole) 
		{
			printf( "%s" , mszPrintBuff );
		}

		//若要求写文件且设置了日志文件名,则把日志信息写入文件中 
		if ((muTraceOptions & AppendToFile) \
			&& (strlen(mszLogFileName) > 1))
		{
			//fanyunfeng 直接输出到文件或者输出到临时缓冲区 
#ifdef LOG_OUTPUT_TO_FILE_DIRECT
			if (lfpTraceFile_ == NULL)
				lfpTraceFile_ = fopen(mszLogFileName,"a");  

			if(lfpTraceFile_ != NULL)
			{
				if(GetFileSize(lfpTraceFile_) > muiLogFileSize)
				{
					fclose(lfpTraceFile_);
					lfpTraceFile_ = NULL;
					SetLogFileName(mszLogFileNamePre);
					lfpTraceFile_ = fopen(mszLogFileName,"a");  
				}
			}

			if (lfpTraceFile_!= NULL)
			{
				fprintf(lfpTraceFile_,"%s",mszPrintBuff);
				fflush(lfpTraceFile_);
			}
#else  //LOG_OUTPUT_TO_FILE_DIRECT
			AddToLogBuffer();
#endif //LOG_OUTPUT_TO_FILE_DIRECT
		}
	}
	catch( ... )
	{
	}
}

void CDebugTrace::AssertFail(char * strCond,char *strFile, unsigned uLine)
{
	char szMessage[512];    
	char strExePath[256];
	getcurrentPath(strExePath, 200);

	sprintf(szMessage, " Debug Assertion Failed!\n Program: %s   \n File: %s  \n Condition: ASSERT(%s);    \n Line:%d \n\n",
		strExePath,strFile,strCond,uLine);      
	TRACE(1,szMessage);
	exit(0);
}

int CDebugTrace::Flush()
{
	int liRet = 0;

#ifndef  LOG_OUTPUT_TO_FILE_DIRECT
	CAutoLock loLock(moCriticalSection);
    try
    {
        if (mlBufDataLen > 0)
        {
            if (lfpTraceFile_ == NULL)
				lfpTraceFile_ = fopen(mszLogFileName,"a");  

			if(lfpTraceFile_ != NULL)
			{
				if(GetFileSize(lfpTraceFile_) > muiLogFileSize)
				{
					SetLogFileName(mszLogFileNamePre);
					fclose(lfpTraceFile_);
					lfpTraceFile_ = fopen(mszLogFileName,"a");  
				}
			}

            if (lfpTraceFile_ != NULL)
            {
                if(1 != fwrite(mszLogBuffer, mlBufDataLen, 1, lfpTraceFile_))
				{
					liRet = errno;
				}

				//清除缓冲区 
				mlBufDataLen = 0;
				memset(mszLogBuffer, 0, (LOG_BUFFER_LEN + 1));
			}
			else
			{
				liRet = errno;
			}
		}
	}
	catch(...)
	{
		liRet = errno;
	}
#endif // LOG_OUTPUT_TO_FILE_DIRECT 
	return liRet;
}

#ifndef  LOG_OUTPUT_TO_FILE_DIRECT
bool CDebugTrace::AddToLogBuffer()
{
	int liFlushRet = 0;

	if ( mlDataLen > LOG_BUFFER_LEN - mlBufDataLen )
	{
		if (NULL != (liFlushRet = Flush()))
		{
			//清空所有的缓冲区 
			mlBufDataLen = 0;

			//增加一条调试信息 
			struct timeb loSystemTime;
			ftime(&loSystemTime);
			struct tm* lptm = localtime(&loSystemTime.time);

			mlBufDataLen = sprintf(mszLogBuffer,"%02d:%02d:%02d:%03d ***CDebugTrace::AddToLogBuffer 输出缓冲区时失败 日志发生循环覆盖(ENO:%d)\n",
				lptm->tm_hour, lptm->tm_min, lptm->tm_sec, loSystemTime.millitm, liFlushRet);

		}
	}

	//前面已经确认过缓冲区的长度
	//缓冲区足够长 
	memcpy(mszLogBuffer + mlBufDataLen, mszPrintBuff, mlDataLen);
	mlBufDataLen += mlDataLen;
	return true;
}
#endif // LOG_OUTPUT_TO_FILE_DIRECT 

//打印当前堆栈 
void CDebugTrace::TraceStack(void)
{
}

