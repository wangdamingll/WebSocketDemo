#include <unistd.h>
#include "common/debugtrace.h"
#include "websocket_handler.h"


std::list<int> Websocket_Handler::broadcast_list_ ; 
int  Websocket_Handler::listen_fd_; 
bool  Websocket_Handler::first_ = true; 

Websocket_Handler::Websocket_Handler(int fd):
    buff_(),
    status_(WEBSOCKET_UNCONNECT),
    header_map_(),
    fd_(fd),
    request_(new Websocket_Request),
    respond_(new Websocket_Respond)
{
    if(first_){
        listen_fd_=fd_;
        broadcast_list_.push_front(listen_fd_);
        first_=false;
    }else {
        broadcast_list_.push_front(fd_);
    }
}

Websocket_Handler::~Websocket_Handler(){}

int Websocket_Handler::process(){
    if(status_ == WEBSOCKET_UNCONNECT){
        return handshark();
    }
    request_->fetch_websocket_info(buff_);
    request_->print();

    unsigned char* out=NULL;
    size_t         out_len;
    std::string request_msg = request_->get_msg();
    size_t   request_msg_len=request_->get_msg_length();

    //判断request请求opcode类型
    //当为1的时候 正常通信
    uint8_t msg_opcode = request_->get_msg_opcode_();
    if(msg_opcode == WEBSOCKET_TEXT_DATA){
        //组包
        respond_->pack_data((const unsigned char*)request_msg.c_str() ,request_msg_len , WEBSOCKET_FIN_MSG_END , 
                WEBSOCKET_TEXT_DATA , WEBSOCKET_NEED_NOT_MASK , &out, &out_len ); 

        send_data((char*)out);           //回显
        send_broadcast_data((char*)out); //广播
        free(out);
    }else if (msg_opcode == WEBSOCKET_CONNECT_CLOSE){
        request_->print();
        del_broadcast_list();
    }

    request_->reset();                  //请求重置
    memset(buff_, 0, sizeof(buff_));
    return 0;
}

int Websocket_Handler::handshark(){
    char request[1024] = {};
    status_ = WEBSOCKET_HANDSHARKED;
    fetch_http_info();
    parse_str(request);
    memset(buff_, 0, sizeof(buff_));
    return send_data(request);
}

void Websocket_Handler::parse_str(char *request){  
    strcat(request, "HTTP/1.1 101 Switching Protocols\r\n");
    strcat(request, "Connection: upgrade\r\n");
    strcat(request, "Sec-WebSocket-Accept: ");
    std::string server_key = header_map_["Sec-WebSocket-Key"];
    server_key += MAGIC_KEY;

    SHA1 sha;
    unsigned int message_digest[5];
    sha.Reset();
    sha << server_key.c_str();

    sha.Result(message_digest);
    for (int i = 0; i < 5; i++) {
        message_digest[i] = htonl(message_digest[i]);
    }
    server_key = base64_encode(reinterpret_cast<const unsigned char*>(message_digest),20);
    server_key += "\r\n";
    strcat(request, server_key.c_str());
    strcat(request, "Upgrade: websocket\r\n\r\n");
}

int Websocket_Handler::fetch_http_info(){
    std::istringstream s(buff_);
    std::string request;

    std::getline(s, request);
    if (request[request.size()-1] == '\r') {
        request.erase(request.end()-1);
    } else {
        return -1;
    }

    std::string header;
    std::string::size_type end;

    while (std::getline(s, header) && header != "\r") {
        if (header[header.size()-1] != '\r') {
            continue; //end
        } else {
            header.erase(header.end()-1);	//remove last char
        }

        end = header.find(": ",0);
        if (end != std::string::npos) {
            std::string key = header.substr(0,end);
            std::string value = header.substr(end+2);
            header_map_[key] = value;
        }
    }

    return 0;
}

int Websocket_Handler::send_data(char *buff){
    return write(fd_, buff, strlen(buff));
}

int Websocket_Handler::send_broadcast_data(char *buff){
    if(broadcast_list_.empty()){
        printf("无客户端连接!");
        TRACE(LOG_INFO,"无客户端连接!");
        return -1;
    }

    try{
        list<int>::iterator it;
        for(it=broadcast_list_.begin() ; it!=broadcast_list_.end() ; it++){
            if( (*it == fd_)|| (*it == listen_fd_) )
                continue;
            write(*it, buff, strlen(buff));
        }
    }catch(exception &e){
        cout<<e.what()<<endl;
        return -1;
    }

    return 0;
}

int Websocket_Handler::del_broadcast_list(){
    try{
        list<int>::iterator it;
        for(it=broadcast_list_.begin() ; it!=broadcast_list_.end() ; it++){
            if( (*it == fd_)&&( *it !=listen_fd_ )){
                it=broadcast_list_.erase(it);
            }
        }
    }catch(exception &e){
        cout<<e.what()<<endl;
        return -1;
    }

    return 0;
}
