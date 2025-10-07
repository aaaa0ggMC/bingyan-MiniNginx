#include <modules/reverse_proxy.h>
#include <socket_util.h>
#include <fcntl.h>

using namespace alib::g3;
using namespace mnginx::modules;
using namespace mnginx;

thread_local ModReverseProxy ModReverseProxy::proxy;

HandleResult ModReverseProxy::handle(HandlerContext ctx,const ReverseClientConfig & cfg){
    /// just send data to server
    LogFactory & lg = *proxy.lg;
    ctx.request.headers["Host"] = "localhost:7000";
    ctx.request.headers["X-Forwarded-For"] = "127.0.0.1";
    ctx.request.headers["X-Forwarded-Host"] = "localhost:9191";
    {
        auto path = ctx.get_param("path");
        if(path)ctx.request.url.parse_raw_url(*path);
    }

    auto& client = proxy.clients.try_emplace(ctx.client.client_id,ReverseClient(ctx.fd)).first->second;
    if(!client.config.modified){
        client.config.modify();
        client.config = cfg; // apply config to new client
    }

    if(client.send_data(ctx.request.generate()) < 0)return HandleResult::Close;
    return client.reverse_proxy();
}

int ReverseClient::setup(){
    if(client_fd != -1)return 0;
    client_fd = socket(AF_INET,SOCK_STREAM,0);
    if(client_fd == -1)return -1;
    fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
    return 0;
}

int ReverseClient::send_data(const std::pmr::vector<char> & data){
    if(connect_server() != 0)return -1;
    return send(client_fd,data.data(),data.size(),MSG_NOSIGNAL);
}

int ReverseClient::connect_server(){
    if(connected)return 0;
    if(client_fd != -1)close(client_fd);
    client_fd = -1;
    setup();
    buffer.clear();
    fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) & ~O_NONBLOCK);
    if(connect(client_fd,(struct sockaddr*)&config.target, sizeof(config.target)) < 0){
        fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
        return -1;
    }
    fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
    connected = true;
    return 0;
}

HandleResult ReverseClient::reverse_proxy(){
    if(connect_server() < 0)return HandleResult::Close; // if client was closed,this wont reconnect
    char buf[mod_reverse_proxy_buffer_size] = {0};

    while(true){
        int ret = read(client_fd,buf,sizeof(buf));
        if(ret > 0){
            if(send(reverse_fd,buf,ret,MSG_NOSIGNAL) < 0){
                return HandleResult::Close;
            }
        }else if(ret == 0){ // connection was closed
            return HandleResult::Close; // close the connection with the client
        }else if(ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)){
            break;
        }else{
            return HandleResult::Close;
        }
    }
    return HandleResult::AlreadySend;
}