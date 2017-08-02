
#include "CriticalSection.h"
#include "debugtrace.h"

CCriticalSection::CCriticalSection()
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mMutex, &attr);
}

CCriticalSection::~CCriticalSection()
{
	pthread_mutex_destroy(&mMutex);
}

CAutoLock::CAutoLock(CCriticalSection& aoSection):moSection(aoSection)
{
	moSection.Enter();
}
CAutoLock::~CAutoLock()
{
	moSection.Leave();
}

/*CEvent::CEvent(const char* apName)
:mbCreated(false)
{
	mstrName = apName;
	//TRACE(5,"CEvent::CEvent() construct:"<<apName);
	//mhCond_t = PTHREAD_COND_INITIALIZER;
}

CEvent::~CEvent()
{	
	//TRACE(5,"CEvent::~CEvent() desconstruct:"<<mstrName.c_str());

#endif
}

//创建事件 
//Linux下不支持手动设置方式和初始化事件状态，这两个参数无效 
bool CEvent::Create(bool bManualReset , bool bInitialState )
{
	//TRACE(5,"CEvent::Create,munualL"<<bManualReset<<",init state:"<<bInitialState<<",name:"<<mstrName.c_str());  
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutex_init(&mhMutex, &attr);
	pthread_mutexattr_destroy(&attr);
	pthread_cond_init(&mhCond_t, NULL); 
	mbCreated = true;
	return true;
}

int CEvent::WaitForEvent(uint32_t dwMilliseconds)
{
	//TRACE(5,"CEvent::WaitForEvent,seconds:"<<dwMilliseconds<<",name:"<<mstrName.c_str());
	if (dwMilliseconds == (uint32_t)-1)
	{
		//使线程阻塞在一个条件变量的互斥锁上，无条件等待
		pthread_mutex_lock(&mhMutex);
		pthread_cond_wait(&mhCond_t, &mhMutex);
		pthread_mutex_unlock(&mhMutex);
		return 0;
	}

	struct timeval now;       
	struct timespec timeout; 

	pthread_mutex_lock(&mhMutex);		//Lock
	//取当前时间
	gettimeofday(&now, NULL); 
	//准备时间间隔值        
	timeout.tv_sec = now.tv_sec + dwMilliseconds / 1000; 
	timeout.tv_nsec = ((now.tv_usec + dwMilliseconds) % 1000) * 1000;        

	//使线程阻塞在一个条件变量的互斥锁上，计时等待
	int ldwResult = pthread_cond_timedwait(&mhCond_t, &mhMutex, &timeout);
	pthread_mutex_unlock(&mhMutex);		//UnLock
	if(ldwResult == ETIMEDOUT)
	{
		return -1;
	}
	return 0;
}

//设置事件为有信号
void CEvent::SetEvent()
{
	//TRACE(5,"CEvent::SetEvent, ..."<<",name:"<<mstrName.c_str());
	pthread_mutex_lock(&mhMutex);
	//唤醒所有被阻塞在条件变量mhCond_t上的线程。
	pthread_cond_broadcast(&mhCond_t);
	pthread_mutex_unlock(&mhMutex);
}

//重新设置事件为无信号
void CEvent::ResetEvent()
{
	//TRACE(5,"CEvent::ResetEvent, ..."<<",name:"<<mstrName.c_str());
}

//关闭事件
void CEvent::Close()
{
	//TRACE(5,"CEvent::Close, ..."<<",name:"<<mstrName.c_str());
	if(mbCreated)
	{
	pthread_cond_destroy(&mhCond_t);
	pthread_mutex_destroy(&mhMutex);
	mbCreated = false;
	}
}*/

Conditional::Conditional(const char *id)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutex_init(&_mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	pthread_cond_init(&_cond, NULL);
	miDataCount = 0;
}

Conditional::~Conditional()
{
	pthread_cond_destroy(&_cond);
	pthread_mutex_destroy(&_mutex);
	miDataCount = 0;
}

bool Conditional::tryEnterMutex(void)
{
	if(pthread_mutex_trylock(&_mutex) != 0)
		return false;
	return true;
}

void Conditional::enterMutex(void)
{
	pthread_mutex_lock(&_mutex);
}

void Conditional::leaveMutex(void)
{
	pthread_mutex_unlock(&_mutex);
}

void Conditional::signal(bool broadcast)
{
	enterMutex();
	miDataCount++;
	if(broadcast)
		pthread_cond_broadcast(&_cond);
	else
		pthread_cond_signal(&_cond);
	leaveMutex();
}

bool Conditional::wait(timeout_t timeout, bool locked)
{
	struct timespec ts;
	int rc;

	if(!locked)
		enterMutex();
	if(!timeout) 
	{
		if(miDataCount-- <= 0)
		{
			pthread_cond_wait(&_cond, &_mutex);
		}

		if(!locked)
			leaveMutex();
		return true;
	}
	getTimeoutEx(&ts, timeout);
	rc = pthread_cond_timedwait(&_cond, &_mutex, &ts);
	if(!locked)
		leaveMutex();
	if(rc == ETIMEDOUT)
		return false;
	return true;
}

