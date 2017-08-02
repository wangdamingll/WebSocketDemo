#pragma once
#pragma warning(disable: 4996)
//#pragma pack(push, 1)
#ifdef _MSC_VER 
#define _STRING_INT64 "%I64d"
#else
#define _STRING_INT64 "%lld"
#endif

#include <iostream>
#include <algorithm>
#include <stdint.h>

#include <unistd.h>
#include <arpa/inet.h>
//#include <netinet/in.h>
#include <bits/sockaddr.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/time.h>

#ifndef INT64 
#define INT64 long long
#endif
#ifndef HANDLE
#define HANDLE void *
#endif

typedef int SOCKET;
typedef int	INT;
typedef char CHAR;
typedef float FLOAT;
typedef unsigned int UINT;
typedef unsigned long ULONG;
#define DWORD UINT
//typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned short WCHAR;
typedef unsigned char BYTE;

typedef long LONG;
typedef unsigned long long UINT64;
typedef long HRESULT;
#define closesocket ::close
#define OUT
#define IN
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
//#ifndef IPPROTO_TCP
//#define IPPROTO_TCP 6
//#endif
// #define max(a,b)    (((a) > (b)) ? (a) : (b))
// #define min(a,b)    (((a) < (b)) ? (a) : (b))

typedef struct sockaddr_in SOCKADDR_IN;

static void epoll_release(int __epfd)
{
	close(__epfd);
}

static void zero_memory( char* _buf, int _len ) 
{
	bzero( _buf, _len );
}

static void sockblock(SOCKET s, bool isBlock)
{

	int flags = fcntl (s, F_GETFL);

	if (flags & O_NONBLOCK) {

		if ( isBlock ) fcntl (s, F_SETFL, flags - O_NONBLOCK);
	}
	else {

		if ( ! isBlock ) fcntl ( s, F_SETFL, flags | O_NONBLOCK);
	}

}

static void Sleep(DWORD mSec)
{
	usleep(mSec * 1000);
}

static long long GetTickCount64()
{
	timeval tv;
	//	timezone tz;
	gettimeofday(&tv, NULL);

	return ((long long)tv.tv_sec) * 1000000 + tv.tv_usec;

}
static DWORD GetTickCount()
{
	return DWORD(GetTickCount64() / 1000);
}




#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
static
bool create_dir(const char *_path)
{
    char dir_path[1024 * 10];
    size_t len = strlen(_path);

    if ( len + 1 >= sizeof(dir_path) ) return false;

    strcpy(dir_path, _path);
    if ( dir_path[len - 1] != '/' ) dir_path[len++] = '/';

    for ( int i = 1; i < len; i++ ) {

        if ( dir_path[i] == '/' ) {

            dir_path[i] = '\0';
            if ( access(dir_path, R_OK) != 0 ) {

                if ( mkdir(dir_path, 0755) == -1 ) {
                    printf("%d", errno);

                    return false;
                }
            }
            dir_path[i] = '/';
        }
    }
    return true;

}

//
// some type for function calling
//
#define LOTACALL

#ifndef interface
#define interface struct
#endif

#ifndef LOTAMETHOD
#define LOTAMETHOD(fun)			virtual HRESULT LOTACALL fun
#define LOTAMETHOD_(typ,fun)	virtual typ LOTACALL fun
#endif //LOTAMETHOD

#ifndef LOTAMETHODIMP
#define LOTAMETHODIMP		HRESULT LOTACALL
#define LOTAMETHODIMP_(typ)	typ		LOTACALL
#endif//LOTAMETHODIMP

#ifndef VOLATILE
#define VOLATILE volatile
#endif//VOLATILE 
//
//
//
struct LTPOINT 
{
	int x;
	int y;
};


#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

static bool getcurrentPath(char *buffer, int len)
{

	if ( ! getcwd(buffer, len) ) return false;
	char *ptr = buffer;
	for (; *ptr; ptr++ ) {	if ( *ptr == '\\') *ptr = '/'; }
	if ( ptr != buffer ) { if ( *(ptr - 1) != '/' ) {*ptr = '/'; ptr[1] = '\0';} }

	return true;

}

static WCHAR *w_strcpy( WCHAR *dest, const WCHAR *src)
{
	const WCHAR *pleft = src;
	WCHAR *pright = dest;
	for (; *pright = *pleft; pleft++, pright++ );
	return dest;
}

static size_t w_strlen(const WCHAR *str)
{

	size_t retval = 0;

	while (str[retval]) retval++;

	return retval;

}

static WCHAR *w_strcat(WCHAR *dest, const WCHAR *str)
{
	w_strcpy(&(dest[w_strlen(dest)]), str);
	return dest;
}

static WCHAR *w_strstr(const WCHAR *str, const WCHAR *substr)
{
	for ( ; *str ; str++ ) {
		if ( *str == *substr ) {
			const WCHAR *pHead = str;
			const WCHAR *pSub = substr;
			for ( ; *pSub; pSub++, pHead++ ) if ( *pSub != *pHead ) break;
			if ( *pSub == 0 ) return (WCHAR *)str;
		}
	}
	return 0;
}

static char *WstrToStr(char *dest, const WCHAR *src)
{
	char *pright = dest;
	const WCHAR *pleft = src;
	for (; *pright = (char)*pleft; pright++, pleft++ );
	return dest;
}

static WCHAR *strToWstr(WCHAR *dest, const char *src)
{
	WCHAR *pright = dest;
	const char *pleft = src;
	for ( ; *pright = *pleft; pright++, pleft++ );
	return dest;
}



static DWORD atoi32(const char *str)
{
	DWORD retval = 0;
	for ( ; *str; str++ ) {
		if ( (unsigned char)(*str - '0') > 9 ) break;
		retval = retval * 10 + (*str - '0');
	}
	return retval;
}

static UINT64 atoi64(const char *str)
{
	UINT64 retval = 0;
	for ( ; *str; str++ ) {
		if ( (unsigned char)(*str - '0') > 9 ) break;
		retval = retval * 10 + (*str - '0');
	}
	return retval;
}

static void _btoa(const unsigned char *binary, int binlen, char *str, int buflen) 
{

	char *charTable = (char *)"0123456789abcdef";//ABCDEF";
	int writeLen = (std::min)(binlen, buflen >> 1);

	for ( int i = 0; i < writeLen; i++ ) {

		str[i << 1] = charTable[binary[i] >> 4];
		str[(i << 1) + 1] = charTable[binary[i] & 0x0f];
	}

	if ( (writeLen << 1) < buflen ) str[writeLen << 1] = '\0';

}
static unsigned char _hexval(unsigned char _ch)
{
	if ( (unsigned char)(_ch - '0') <= '9' - '0' ) return _ch - '0';
	if ( (unsigned char)( (_ch | ('a' ^ 'A')) - 'a') <= 'f' - 'a' ) return 10 + (_ch | ('a' ^ 'A')) - 'a';
	return 0xFF;

// 	if ( ch < '0' ) return 0xFF;
// 	if ( ch <= '9' ) return ch - '0';
// 	if ( ch < 'A' ) return 0xFF;
// 	if ( ch <= 'F' ) return ch - 'A' + 10;
// 	if ( ch < 'a' ) return 0xFF;
// 	if ( ch <= 'f' ) return ch - 'a' + 10;
// 	return 0xFF;
}
static unsigned char _hex_val(unsigned char _ch)
{
    if ( (unsigned char)(_ch - '0') < 10 ) return _ch - '0';
    if ( (unsigned char)( (_ch | ('a' ^ 'A')) - 'a') <= 'f' - 'a' ) return 10 + (_ch | ('a' ^ 'A')) - 'a';
    return 0xFF;
}
static unsigned long _atohex(const char *_val)
{
    unsigned long retval = 0;
    for (; *_val; _val++ ) {
        unsigned char hex_val = _hex_val(*_val);
        if ( hex_val == 0xFF ) return retval;
        retval = (retval << 4) + hex_val;
    }
    return retval;
}
static int _atob(const char *str, unsigned char *binary, int len)//����ת���ɹ���byte����
{
	int i = 0;
	for (; i < len; i++ ) {

		char ch = str[2 * i];
		char cl = str[2 * i + 1];
		if ( (ch == 0) || (cl == 0) ) break;

		unsigned char byte_heigh = _hexval(ch);
		if ( byte_heigh == 0xFF ) break;
		unsigned char byte_low = _hexval(cl);
		if ( byte_low == 0xFF ) break;
		binary[i] = (byte_heigh << 4) + byte_low;
	}
	return i;
}
//#pragma pack(pop)

static int getProcessId()
{
	int pid = getpid();
	return pid;
}

static uint32_t getSecondsSince1970 ()
{
	return uint32_t(time(NULL));
}
