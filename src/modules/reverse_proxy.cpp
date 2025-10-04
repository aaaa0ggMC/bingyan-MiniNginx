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
    auto& client = proxy.clients.try_emplace(ctx.client.client_id,ReverseClient(ctx.fd)).first->second;
    
    client.send_data(ctx.request.generate());
    return client.reverse_proxy();
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

HandleResult ReverseClient::reverse_proxy(){
    connect_server(); // if client was closed,this wont reconnect
    char buf[1024] = {0};

    while(true){
        int ret = read(client_fd,buf,1024);
        if(ret > 0){
            std::cout << "reverse proxied" << std::endl;
            send(reverse_fd,buf,ret,0);
        }else if(ret == 0){ // connection was closed
            return HandleResult::Close; // close the connection with the client
        }else if(ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)){
            break;
        }else return HandleResult::Close;
    }
    return HandleResult::AlreadySend;
}