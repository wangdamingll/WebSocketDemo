#ifndef __WEBSOCKET_HANDLER__
#define __WEBSOCKET_HANDLER__

#include <arpa/inet.h>
#include <iostream>
#include <map>
#include <list>
#include <sstream>
#include "base64.h"
#include "sha1.h"
#include "debug_log.h"
#include "websocket_request.h"
#include "websocket_respond.h"

//魔法字符串，握手请求使用
#define MAGIC_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

//用来描述是否已经经过SHA1处理
//握手时使用
enum WEBSOCKET_STATUS {
	WEBSOCKET_UNCONNECT         = 0,
	WEBSOCKET_HANDSHARKED       = 1,
};

//用于描述消息是否结束
enum WEBSOCKET_FIN_STATUS {
     WEBSOCKET_FIN_MSG_END      =1,  //该消息为消息尾部
     WEBSOCKET_FIN_MSG_NOT_END  =0,  //还有后续数据包
};

//用于描述消息类型
//目前websocket未定义的类型已省略
enum WEBSOCKET_OPCODE_TYPE {
     WEBSOCKET_APPEND_DATA      =0X0, //表示附加数据
     WEBSOCKET_TEXT_DATA        =0X1, //表示文本数据
     WEBSOCKET_BINARY_DATA      =0X2, //表示二进制数据
     WEBSOCKET_CONNECT_CLOSE    =0X8, //表示连接关闭
     WEBSOCKET_PING_PACK        =0X9, //表示ping
     WEBSOCKET_PANG_PACK        =0XA, //表示表示pong
};

//用于描述数据是否需要掩码
//客户端发来的数据必须要掩码处理
//服务器返回的数据不需要掩码处理
enum WEBSOCKET_MASK_STATUS {
     WEBSOCKET_NEED_MASK        =1,  //需要掩码处理
     WEBSOCKET_NEED_NOT_MASK    =0,  //不需要掩码处理
};

typedef std::map<std::string, std::string>  HEADER_MAP;

class Websocket_Handler{
public:
	Websocket_Handler(int fd);
	~Websocket_Handler();
	int          process();                 //回显、广播逻辑处理
	inline char *getbuff();                 //用来返回客户端请求的信息

private:
	int     handshark();                    //用于与客户端握手的SHA1操作
	void    parse_str(char *request);       //解析握手的请求
	int     fetch_http_info();              //处理请求   
	int     send_data(char *buff);          //发送回显数据
    int     send_broadcast_data(char *buff);//发送广播数据
    int     del_broadcast_list();           //删除广播列表中的fd_文件描述符对应的客户端

private:
	char                    buff_[2048];    //用来临时缓存客户端发来请求的详细数据
	WEBSOCKET_STATUS        status_;        //表示是否经过了SHA1处理
	HEADER_MAP              header_map_;    //用来处理SHA1处理后的key-value
	int                     fd_;            //客户端连接的fd
	Websocket_Request *     request_;       //请求处理类
    Websocket_Respond *     respond_;       //应答处理类
    static std::list<int>   broadcast_list_;//广播列表
	static int              listen_fd_;     //epoll监听的listen_fd
    static bool             first_ ;        //表示构造函数是否是第一次初始化
};

//返回客户端请求的详细信息
inline char *Websocket_Handler::getbuff(){
	return buff_;
}

#endif
