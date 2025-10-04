#include <server.h>
#include <fcntl.h>
#include <string.h>
#include <socket_util.h>

using namespace mnginx;
using namespace alib::g3;

void Server::setup(){
    int len = sizeof(config.address);

    server_fd = Util::create_nonblocking_tcp_socket();
    if(server_fd == -1){
        lg_err(LOG_CRITI) << "Cannot create server socket: " << strerror(errno) << endlog;
        return;
    }

    // resuable port
    if(Util::set_reuse_addr(server_fd) < 0){
        lg_err(LOG_WARN) << "Failed to set SO_REUSEADDR: " << strerror(errno) << endlog;
    }

    if(bind(server_fd,(sockaddr*)&config.address,sizeof(config.address)) == -1){
        lg_err(LOG_CRITI) << "Cannot bind the port " << ntohs(config.address.sin_port) << " for the server:" << strerror(errno) << endlog;
        close_server();
        return;
    }

    // n is the count of backlog,other clients will be suspended
    if(listen(server_fd,config.backlog_count) == -1){
        lg_err(LOG_CRITI) << "Listen failed on port " << ntohs(config.address.sin_port) << ": " << strerror(errno) << endlog;
        std::exit(-1);
    }
    
    // Epoll settings
    epoll.resize(config.epoll_event_list_size);

    // listen for connections
    epoll.add(server_fd,EPOLLIN); // care about read ==> client connections,no EPOLLET,this will damage the connection

    /// all set,now let's wait for client in the run function 
    lg_acc(LOG_INFO) << "Server listening on " << 
    mnginx::Util::ip_to_str(config.address.sin_addr.s_addr) << ":" << 
    ntohs(config.address.sin_port) << endlog;
}

void Server::run(){
    if(server_fd < 0)return;
    for(auto & fn : mods.init){
        if(fn)fn(server_fd,epoll,establishedClients,lg_acc,lg_err);
    }
    Clock timer;
    while(true){
        auto [ok,ev_view] = epoll.wait(config.epoll_wait_interval_ms);
        if(ok){
            if(ev_view.size()){
                for(auto & ev : ev_view){
                    if(ev.data.fd == server_fd){
                        accept_connections();
                        continue;
                    }
                    handle_client(ev);
                }
            }else{ // timeout
                // lg(LOG_WARN) << "Waited to timeout but received no clients!" << endlog;
            }
        }else{
            lg_err(LOG_ERROR) << "Epoll wait failed: " << strerror(errno) << endlog;
        }

        // at least wait 
        if(timer.getOffset() > config.module_timer_at_least_wait_ms){
            for(auto & tm : mods.timer){
                if(tm)tm(timer.getOffset());
            }
            timer.clearOffset();
        }
    }
}

void Server::close_server(){
    if(server_fd > 0)close(server_fd);
    server_fd = -1;
}

void Server::accept_connections(){
    int client_fd = -1;
    struct sockaddr_in client_info;
    int sizelen = sizeof(sockaddr_in);
    while(true){
        if((client_fd = accept(server_fd,(sockaddr*)&client_info,(socklen_t*)&sizelen)) == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                // Accept work finished
                break;
            }else lg_err(LOG_ERROR) << "Accept failed: " << strerror(errno) << endlog;
        }else{
            fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
            lg_acc(LOG_INFO) << "New client connected: fd[" << client_fd 
            << "] addr[" << Util::ip_to_str(client_info.sin_addr.s_addr) << "]" << endlog;
            establishedClients.emplace(client_fd,ClientInfo());
            // add epoll events
            epoll.add(client_fd,EPOLLIN | EPOLLET);
        }
    }
}

void Server::handle_client(epoll_event & ev){
    thread_local static char buffer[mnginx_handle_buffer_size]; // thread_local is just a decorative item I think
    int client_fd = ev.data.fd;
    bool shouldClose = false;

    if(!(ev.events & EPOLLIN))return;
    
    if(ev.events & EPOLLRDHUP){
        lg_acc(LOG_INFO) << "Client orderly shutdown: fd[" << client_fd << "]" << endlog;
        shouldClose = true;
    }
    
    if(ev.events & EPOLLHUP){
        lg_err(LOG_WARN) << "Client hung up: fd[" << client_fd << "]" << endlog;
        shouldClose = true;
    }
    
    if(ev.events & EPOLLERR){
        lg_err(LOG_ERROR) << "Client connection error: fd[" << client_fd << "]" << endlog;
        shouldClose = true;
    }

    if(shouldClose){
        cleanup_connection(client_fd);
        return;
    }

    // @todo remember to add while(1) and write it into buffer
    auto it = establishedClients.find(client_fd);
    if(it == establishedClients.end()){ // no way!
        // i think there's no way this code could be run
        lg_err(LOG_ERROR) << "Unregistered client connection: fd[" << client_fd << "]" << endlog;
        cleanup_connection(client_fd);
        return;
    }
    do{
        size_t count = read(client_fd, buffer, sizeof(buffer));
        if(count == 0){
            lg_acc(LOG_INFO) << "Client closed connection: fd[" << client_fd << "]" << endlog;
            shouldClose = true;
        }else if(count == -1 && errno != EAGAIN){
            lg_err(LOG_ERROR) << "Read failed: " << strerror(errno) << " fd[" << client_fd << "]" << endlog;
            shouldClose = true;
        }else if(count > 0){
            lg_acc(LOG_INFO) << "Received " << count << " bytes from client: fd[" << client_fd << "]" << endlog;
            it->second.buffer.insert(it->second.buffer.end(),buffer,buffer + count);
        }
        if(count == sizeof(buffer))continue; //maybe there's more data
    }while(false);

    // @todo if the size of the buffer exceedes 8MB(can be configured later),we treat it as an sus one
    if(config.http_package_max_size && it->second.buffer.size() > config.http_package_max_size){
        lg_err(LOG_ERROR) << "Client message too large: " 
        << it->second.buffer.size() << " bytes, fd[" 
        << client_fd << "]" << endlog;
        shouldClose = true;
    }

    if(shouldClose){
        cleanup_connection(client_fd);
    }else{
        // try to unpack the buffer and extract data
        std::string_view buffer_str (it->second.buffer.begin() + it->second.find_last_pos,it->second.buffer.end());
        if(it->second.pending){
            // @todo WIP
            handle_pending_request(it->second,client_fd);
            //maybe we can deal another message then,so no return here
        }

        auto pos = buffer_str.find("\r\n\r\n");

        if(pos != std::string_view::npos){
            pos += it->second.find_last_pos + 4;
            // We dont care about CLEN or OTHERS,now we just analyse the header
            // the first of the request begins with an alphabet,so we ignore others
            int i = 0;
            std::string_view data_chunk (it->second.buffer.begin(),it->second.buffer.begin() + pos);
            for(;(data_chunk[i] < 'A' || data_chunk[i] > 'Z') && i < data_chunk.size();++i)continue;
            if(i >= data_chunk.size()){
                // invalid connection data,so we close the connection
                lg_err(LOG_WARN) << "Invalid HTTP data format from client: fd[" << client_fd << "]" << endlog;
                cleanup_connection(client_fd) ;
                return;
            }
            std::string_view valid_chunk (it->second.buffer.begin() + i,it->second.buffer.begin() + pos);
            it->second.pending_request = HTTPRequest();
            if(it->second.pending_request.parse(valid_chunk) != ParseCode::Success){
                // invalid HTTP Method
                lg_err(LOG_WARN) << "Invalid HTTP request from client: fd[" << client_fd << "]" << endlog;
                cleanup_connection(client_fd);
                return;
            }else{
                lg_acc(LOG_INFO) << HTTPRequest::getMethodString(it->second.pending_request.method) 
                 << " " << it->second.pending_request.url.full_path() 
                 << " HTTP/" << it->second.pending_request.version.major 
                 << "." << it->second.pending_request.version.minor << endlog;
            }
            // note that all the string_view will have memeory issue after this call
            it->second.buffer.erase(it->second.buffer.begin(),it->second.buffer.begin() + pos); 
            it->second.find_last_pos = 0;
            it->second.pending = true;
            // simple methods(GET) have no message body
            handle_pending_request(it->second,client_fd);
        }else{
            it->second.find_last_pos = it->second.buffer.size();
        }
    }
}

bool Server::cleanup_connection(int fd){
    auto it = establishedClients.find(fd);
    if(it == establishedClients.end()){
        lg_err(LOG_WARN) << "Cleanup unregistered fd: " << fd << endlog;
        return false;
    }else{
        // cleanup epoll data
        epoll.del(fd);
        close(fd); // close client fd from server
        establishedClients.erase(it);
        lg_acc(LOG_INFO) << "Connection closed: fd[" << fd << "]" << endlog;
        return true;
    }
}

void Server::handle_pending_request(ClientInfo & client,int fd){
    auto & req = client.pending_request;
    thread_local static HTTPResponse response = []{
        HTTPResponse resp;
        resp.data.emplace();
        resp.version.major = 1;
        resp.version.minor = 1;
        return resp;
    }();
    using M = HTTPRequest::HTTPMethod;
    bool pass_to_handler = false;

    switch(req.method){
    case M::CONNECT:
    case M::TRACE:
    case M::OPTIONS:
    case M::HEAD:
    case M::GET:{
        if(req.headers.find(KEY_Content_Length) != req.headers.end() ||
           req.headers.find(KEY_Transfer_Encoding) != req.headers.end()){ //bad request
            lg_err(LOG_WARN) << HTTPRequest::getMethodString(req.method) 
                << " request with body from fd[" << fd << "]" << endlog;
            send_message_simp(fd,HTTPResponse::StatusCode::BadRequest,
                HTTPResponse::get_status_description(HTTPResponse::StatusCode::BadRequest));
            cleanup_connection(fd);
            return;
        }else{ //nice guys now we can send something useful back
            pass_to_handler = true;
        }
        client.pending = false;
        break;
    }
    case M::PUT:
    case M::PATCH:
    case M::DELETE:
    case M::POST:{ // there's data in the message body,so we need to extract the data
        bool ok = false;
        auto p = req.headers.find(KEY_Transfer_Encoding);
        if(!client.pending_request.data)client.pending_request.data.emplace();
        if(p != req.headers.end()){
            if(p->second.compare("chunked")){
                lg_err(LOG_WARN) << "Unsupported Transfer-Encoding: " << p->second << endlog;
                return;
            }
            // chunked mode
            // read existing data
            while(true){
                std::string_view buffer (client.buffer.begin(),client.buffer.end());
                char * endptr = nullptr;
                size_t pos = buffer.find("\r\n");
                if(pos == std::string_view::npos)return; // data is not well prepared
                int count = strtol(buffer.data(),&endptr,16); // @note the chunked data is a hex number
                if(endptr == buffer.data()){ // invalid character
                        lg_err(LOG_WARN) << "Bad chunked encoding format from fd[" << fd << "]" << endlog;
                        send_message_simp(fd,HTTPResponse::StatusCode::BadRequest,
                            HTTPResponse::get_status_description(HTTPResponse::StatusCode::BadRequest));
                        cleanup_connection(fd);
                }
                if(((int)(buffer.size()) - 4 - count - pos) >= 0){ // why minus 4? ==> two \r\n
                    if(count)client.pending_request.data->insert(client.pending_request.data->end(),
                            buffer.begin() + (pos+2),buffer.begin() + (pos+2+count));
                    // check whether it ends with \r\n]
                    if(strncmp("\r\n",buffer.data() + (pos+2+count),2)){
                        lg_err(LOG_WARN) << "Invalid chunked encoding terminator from fd[" 
                            << fd << "]" << endlog;
                        send_message_simp(fd,HTTPResponse::StatusCode::BadRequest,
                            HTTPResponse::get_status_description(HTTPResponse::StatusCode::BadRequest));
                        cleanup_connection(fd);
                        return;
                    }
                    // update buffer
                    client.buffer.erase(client.buffer.begin(),client.buffer.begin() + (pos+4+count));
                    if(count == 0){
                        // that means the chunk ended
                        ok = true;
                        break;
                    }
                }else return;
            }
        }
        if(!ok){
            p = req.headers.find(KEY_Content_Length);
            if(p != req.headers.end()){
                char * endptr = nullptr;
                int length = strtol(p->second.data(),&endptr,10);
                if(endptr != p->second.data()){ // actually have data here!
                    if(client.buffer.size() < length){
                        return; // data not well prepared
                    }
                    client.pending_request.data->insert(
                        client.pending_request.data->end(),
                        client.buffer.begin(),
                        client.buffer.begin() + length
                    );
                    // update buffer
                    client.buffer.erase(client.buffer.begin(),client.buffer.begin() + length);
                }// else just ignore,anyway,the task is not pending now
            }
        }
        client.pending = false;
        pass_to_handler = true;
        break;
    }
    default: //no way!
        break;
    }

    if(pass_to_handler){
        HandlerFn fn;
        static thread_local std::pmr::vector<std::pmr::string> vals;
        static thread_local std::pmr::unordered_map<std::string_view,int> mapper;

        vals.clear();
        mapper.clear();

        auto result = handlers.parseURL(client.pending_request.url.main_path(),fn,vals,mapper);
        if(result == StateTree::ParseResult::OK){
            response.reset();
            auto result = fn(HandlerContext(fd,client.pending_request,vals,response,client,mapper));
            if(result == HandleResult::Close){
                // @todo add more detailed information instead of a NOT FOUND
                send_message_simp(fd,HTTPResponse::StatusCode::NotFound,"Not Found!");
                cleanup_connection(fd);
            }else if(result != HandleResult::AlreadySend)send_message(fd,response);
        }else{
            lg_acc(LOG_INFO) << "No handler for: " << 
                client.pending_request.url.main_path() << endlog;
            send_message_simp(fd,HTTPResponse::StatusCode::NotFound,"Not Found!");
        }
    }
}

ssize_t Server::send_message_simp(int fd,HTTPResponse::StatusCode code,std::string_view stat_str){
    thread_local static HTTPResponse resp;
    resp.version.major = 1;
    resp.version.minor = 1;
    resp.status_code = code;
    resp.status_str.assign(stat_str.begin(),stat_str.end());
    // checkout will auto do this
    // resp.headers[KEY_Content_Length] = "0";

    return send_message(fd,resp);
}

ssize_t Server::send_message(int fd,HTTPResponse & resp,HTTPResponse::TransferMode mode){
    // auto checkout
    static thread_local std::pmr::vector<char> data;
    resp.checkout_data(mode);
    resp.generate_to(data);
    return send(fd,data.data(),data.size(),MSG_NOSIGNAL);
}