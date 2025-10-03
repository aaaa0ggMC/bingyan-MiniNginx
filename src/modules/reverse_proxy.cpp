#include <modules/reverse_proxy.h>
#include <iostream>
#include <fcntl.h>

using namespace alib::g3;
using namespace mnginx::modules;
using namespace mnginx;

thread_local ModReverseProxy ModReverseProxy::proxy;

HandleResult ModReverseProxy::handle(HandlerContext ctx){
    /// just send data to server
    LogFactory & lg = *proxy.lg;
    ctx.request.headers["Host"] = "localhost:7000";
    ctx.request.headers["X-Forwarded-For"] = "127.0.0.1";
    ctx.request.headers["X-Forwarded-Host"] = "localhost:9191";
    //ctx.request.url.parse_raw_url("/Blog/");
    
    auto& client = proxy.clients.try_emplace(ctx.client.client_id,ReverseClient(ctx.fd)).first->second;
    
    client.send_data(ctx.request.generate());
    lg(LOG_ERROR) << "Watch ME!" << endlog;
    client.reverse_proxy();
    lg(LOG_ERROR) << "Well DONT Watch ME!" << endlog;

    return HandleResult::AlreadySend;
}

int ReverseClient::setup(){
    if(client_fd != -1)return 0;
    client_fd = socket(AF_INET,SOCK_STREAM,0);
    if(client_fd == -1)return -1;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7000);
    inet_pton(AF_INET,"127.0.0.1",&server_addr.sin_addr);
    fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
    return 0;
}

void ReverseClient::send_data(const std::pmr::vector<char> & data){
    connect_server();
    send(client_fd,data.data(),data.size(),0);
}

int ReverseClient::connect_server(){
    if(connected)return 0;
    if(client_fd != -1)close(client_fd);
    client_fd = -1;
    std::cout << "Created socket for" << reverse_fd<< std::endl;
    setup();
    buffer.clear();
    fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) & ~O_NONBLOCK);
    if(connect(client_fd,(struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
        return -1;
    }
    fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
    connected = true;
    return 0;
}

int ReverseClient::reverse_proxy(){
    connect_server(); // if client was closed,this wont reconnect
    char buf[1024] = {0};

    while(true){
        int ret = read(client_fd,buf,1024);
        if(ret > 0){
            std::cout << "reverse proxied" << std::endl;
            send(reverse_fd,buf,ret,0);
        }else if(ret == 0){ // connection was closed
            return -1; // close the connection with the client
        }else if(ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)){
            break;
        }else return -1;
    }
    return 0;
}
/** @cached code  
    // size_t lastpos = 0;
    // bool headerok = false;
    // int64_t mode = -1;
    // int64_t sent_already = 0;
      auto var_reset = [&]{
        headerok = false;
        mode = -1;
        lastpos = 0;
        sent_already = 0;
    };
  if(headerok){
    std::cout << "Send " << mode << std::endl;
    if(mode > 0){
        int send_num;
        if(sent_already + bsize >= mode)send_num = mode - sent_already;
        else send_num = bsize;
        send(reverse_fd,buffer.data(),send_num,0);
        sent_already += send_num;
        buffer.erase(buffer.begin(),buffer.begin() + send_num);

        if(sent_already >= mode){
            std::cout << "Finished" << std::endl;
            // fin
            var_reset();
        }
    }else{
        auto pos = sbuf.find("0");
        if(pos == std::string_view::npos){
            // send these
            send(reverse_fd,buffer.data(),buffer.size(),0);
            buffer.clear();
            continue;
        }else{
            if(pos+5 > sbuf.size()){ // located at least to \r\n\r\n
                continue; // cannot be
            }else if(!strncmp(&sbuf[pos],"0\r\n\r\n",5)){
                // fin 
                send(reverse_fd,buffer.data(),pos + lastpos + 5,0);
                buffer.erase(buffer.begin(),buffer.begin() + (pos + lastpos + 5));
                var_reset();
            }
        }
    }
}else{
    auto p = sbuf.find("\r\n\r\n");
    std::cout << "detected " << p << std::endl;
    if(p == std::string_view::npos){
        lastpos = buffer.size();
    }else{ // the header was discovered
        headerok = true;
        std::string_view header (buffer.begin(),buffer.begin() + p + lastpos + 4); // 4 is \r\n\r\n
        // find if KEY_TE exist
        auto key = header.find(KEY_Transfer_Encoding);
        if(key == std::string_view::npos){
            key = header.find(KEY_Content_Length);
            if(key != std::string_view::npos){ // have message body
                char * endptr;
                size_t value_start = key + KEY_Len_Content_Length + 1;
                // skip addtional spaces
                while(header[value_start] == ' ' || header[value_start] == '\t'){
                    ++value_start;
                    if(value_start >= header.size())break;
                }
                if(value_start >= header.size())return -2;
                mode = strtol(header.data() + value_start,&endptr,10); // 1 is for the ':'
                if(header.data() + value_start == endptr){ 
                    return -2; // send 404 back
                }
                std::cout << "Recognized mode:" << mode << std::endl;
            }else mode = 0; // empty response
        }else mode = -1;
        // OK send data on the fly
        send(reverse_fd,header.data(),header.size(),0);
        buffer.erase(buffer.begin(),buffer.begin() + p + lastpos + 4); /// remove header
        lastpos = 0;
        if(mode == 0){
            // fin, send mext package
            var_reset();
        }
    }
}
 */