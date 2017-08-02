#include "websocket_respond.h"
#include "common/debugtrace.h"

Websocket_Respond::Websocket_Respond(): fin_(),opcode_(), mask_(){}
Websocket_Respond::~Websocket_Respond() { }


int  Websocket_Respond::pack_data(const unsigned char* message,size_t msg_len, uint8_t fin , 
        uint8_t opcode, uint8_t mask, unsigned char** out, size_t* out_len)
{
    if(message == NULL || msg_len<0 || out_len== NULL ){
        TRACE(LOG_ERROR,"pack_data parameters error!");
        return -1;
    }

    int head_length=0;                                  //记录消息头部的长度
    size_t          out_len_tmp = msg_len;              //临时变量
    unsigned char*  out_tmp     = NULL; 

    // 基本一个包可以发送完所有数据
    if(msg_len < 126)                                   //如果不需要扩展长度位, 两个字节存放 fin(1bit) + rsv[3](1bit) + opcode(4bit); 
                                                        //mask(1bit) + payloadLength(7bit);  
        out_len_tmp += 2;  
    else if (msg_len < 0xFFFF)                          //如果数据长度超过126 并且小于两个字节, 我们再用后面的两个字节(16bit) 表示 uint16_t  
        out_len_tmp += 4;  
    else                                                //如果数据更长的话, 我们使用后面的8个字节(64bit)表示 uint64_t  
        out_len_tmp += 8;  

    if (mask & 0x1)                                     //判断是不是1  
        out_len_tmp += 4;                               //4byte 掩码位  

    out_tmp = (unsigned char*)realloc((void*)out_tmp, out_len_tmp);   //长度已确定，可以分配内存

    //做数据处理
    memset(out_tmp, 0, out_len_tmp+1);  
    *out_tmp = fin << 7;                                //开始处理第一个字节的数据                   
    *out_tmp = *out_tmp | (0xF & opcode);               //处理opcode, out_tmp 在 第一个字节开始处

    *(out_tmp + 1) = mask << 7;                         //开始处理第二个字节的数据, out_tmp在第一个字节的起始位置, 设置mask为 1
    if (msg_len < 126)                 
    {  
        *(out_tmp + 1) = *(out_tmp + 1) | msg_len;      //为第二个字节的payload len赋值   
        head_length += 2;                               //head_length标志着已经处理了2个字节的数据
    }  
    else if (msg_len < 0xFFFF)  
    {  
        *(out_tmp + 1) = *(out_tmp + 1) | 0x7E;         //设置第二个字节后7bit为126  
        uint16_t tmp = htons((uint16_t)msg_len);        //转换成网络字节序
        memcpy(out_tmp + 2, &tmp, sizeof(uint16_t));    //给extended payload length 赋值
        head_length += 4;                               //标志着已经处理了4个字节的数据
    }  
    else  
    {  
        *(out_tmp + 1) = *(out_tmp + 1) | 0x7F;         //设置第二个字节后为7bit 127  
        uint16_t tmp = htonl((uint16_t)msg_len);        //转换成网络字节序
        memcpy(out_tmp + 2, &tmp, sizeof(uint16_t));    //给extended payload length 赋值
        head_length += 10;                              //标志着已经处理了10个字节的数据
    }

    //处理掩码
    if (mask & 0x1)                                     //因为此时的mask为 1 ,条件为假
    {  
        head_length += 4;                               //因协议规定, 从服务器向客户端发送的数据, 一定不能使用掩码处理. 所以这边省略  
    }  

    memcpy(out_tmp + head_length, message, msg_len);    //拷贝真实的数据域    
    *(out_tmp + out_len_tmp ) = '\0';                   //消息的最后一个字节设置为'\0'

    *out_len = out_len_tmp;                             //间接赋值
    *out     = out_tmp;

    return 0; 
}
