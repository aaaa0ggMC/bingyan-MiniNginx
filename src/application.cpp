#include <application.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <epoll.h>
#include <unistd.h>
#include <fcntl.h>

using namespace mnginx;

Application::Application():logger{LOG_SHOW_HEAD | LOG_SHOW_THID | LOG_SHOW_TIME | LOG_SHOW_TYPE},lg{"MiniNginx",logger}{
    server_fd = -1;
}

Application::~Application(){
    if(server_fd != -1)close(server_fd);
}

//// Setup Section ////
void Application::setup(){
    setup_general();
    setup_logger();

    setup_server();
}

void Application::setup_general(){
    // Initialize cpp pmr resource pool,note that I'm not very familiar with it
    // I just know how to use
    resource = &pool;
    std::pmr::set_default_resource(resource);
}

void Application::setup_server(){
    int len = sizeof(address);

    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){
        lg(LOG_CRITI) << "Cannot create a socket for the server:" << strerror(errno) << endlog;
        std::exit(-1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(9191);

    // non-block mode
    fcntl(server_fd,F_SETFL,fcntl(server_fd,F_GETFL,0) | O_NONBLOCK);

    if(bind(server_fd,(sockaddr*)&address,sizeof(address)) == -1){
        lg(LOG_CRITI) << "Cannot bind the port " << address.sin_port << " for the server:" << strerror(errno) << endlog;
        std::exit(-1);
    }

    // n is the count of backlog,other clients will be suspended
    if(listen(server_fd,1024) == -1){
        lg(LOG_CRITI) << "Cannot listen the port " << address.sin_port << ":" << strerror(errno) << endlog;
        std::exit(-1);
    }
    
    // Epoll settings
    epoll.resize(1024);

    // listen for connections
    epoll.add(server_fd,EPOLLIN); // care about read ==> client connections,no EPOLLET,this will damage the connection

    /// all set,now let's wait for client in the run function 
}

void Application::setup_logger(){
    logger.appendLogOutputTarget("file",std::make_shared<lot::SplittedFiles>("./data/latest.log",4 * 1024 * 1024));
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
}

//// Main Section ////
void Application::run(){
    while(true){
        auto [ok,ev_view] = epoll.wait(4000);
        if(ok){
            if(ev_view.size()){
                lg(LOG_INFO) << "Received events!" << endlog;
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
            lg(LOG_ERROR) << "Error occured when waiting for messages:" << strerror(errno) << endlog;
        }
    }
}

void Application::accept_connections(){
    int client_fd = -1;
    struct sockaddr_in client_info;
    int sizelen = sizeof(sockaddr_in);
    while(true){
        if((client_fd = accept(server_fd,(sockaddr*)&client_info,(socklen_t*)&sizelen)) == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                lg(LOG_TRACE) << "Accept work finished." << endlog;
                break;
            }else lg(LOG_ERROR) << "Error occured when accepting new client!:" << strerror(errno) << endlog;
        }else{
            fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
            lg(LOG_INFO) << "Established new client: fd[" << client_fd << "] addr[" << client_info.sin_addr.s_addr << "]" << endlog;
            establishedClients.emplace(client_fd,ClientInfo());

            // add epoll events
            epoll.add(client_fd,EPOLLIN | EPOLLET);
        }
    }
}

void Application::handle_client(epoll_event & ev){
    thread_local static char buffer[1024]; // thread_local is just a decorative item I think
    int client_fd = ev.data.fd;
    bool shouldClose = false;

    if(!(ev.events & EPOLLIN))return;
    
    if(ev.events & EPOLLRDHUP){
        lg(LOG_INFO) << "Client " << client_fd << " performed orderly shutdown" << endlog;
        shouldClose = true;
    }
    
    if(ev.events & EPOLLHUP){
        lg(LOG_INFO) << "Client " << client_fd << " hung up" << endlog;
        shouldClose = true;
    }
    
    if(ev.events & EPOLLERR){
        lg(LOG_INFO) << "Client " << client_fd << " connection error" << endlog;
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
        lg(LOG_ERROR) << "The coming client is not recorded!" << endlog; 
        cleanup_connection(client_fd);
        return;
    }
    do{
        size_t count = read(client_fd, buffer, sizeof(buffer));
        if(count == 0){
            lg(LOG_INFO) << "Client closed the connection" << endlog;
            shouldClose = true;
        }else if(count == -1 && errno != EAGAIN){
            lg(LOG_ERROR) << "Error occured when reading data from client:" << strerror(errno) << endlog;
            shouldClose = true;
        }else if(count > 0){
            std::string test(buffer,buffer + count);
            lg(LOG_INFO) << "Received \"" << test << " \"from client" << endlog;
            it->second.buffer.insert(it->second.buffer.end(),buffer,buffer + count);
        }
        if(count == sizeof(buffer))continue; //maybe there's more data
    }while(false);

    // @todo if the size of the buffer exceedes 8MB(can be configured later),we treat it as an sus one
    if(it->second.buffer.size() > 8 * 1024 * 1024){
        lg(LOG_ERROR) << "The client fd[" << client_fd << "] post toos long a message!" << endlog;
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
                cleanup_connection(client_fd);
                return;
            }
            std::string_view valid_chunk (it->second.buffer.begin() + i,it->second.buffer.begin() + pos);
            it->second.pending_request = HTTPRequest();
            lg(LOG_INFO) <<  "We have processed:["  << std::string(valid_chunk.begin(),valid_chunk.end()) << "]" << std::endl;
            if(it->second.pending_request.parse(valid_chunk) != ParseCode::Success){
                // invalid HTTP Method
                cleanup_connection(client_fd);
                return;
            }else{
                lg(LOG_TRACE) << "Successfully parsed a header: method[" <<
                HTTPRequest::getMethodString(it->second.pending_request.method) << "] url[" <<
                it->second.pending_request.url.full_path() << "] version[" << it->second.pending_request.version.major << "."
                << it->second.pending_request.version.minor << "]..." << endlog; 
            }
            // note that all the string_view will have memeory issue after this call
            it->second.buffer.erase(it->second.buffer.begin(),it->second.buffer.begin() + pos); 
            it->second.find_last_pos = 0;
            it->second.pending = true;
            // some simple methods(GET) have no message body
            handle_pending_request(it->second,client_fd);
        }else{
            it->second.find_last_pos = it->second.buffer.size();
        }
    }
}

bool Application::cleanup_connection(int fd){
    auto it = establishedClients.find(fd);
    if(it == establishedClients.end()){
        lg(LOG_WARN) << "Trying to cleanup a fd that is not recorded." << endlog;
        return false;
    }else{
        // cleanup epoll data
        epoll.del(fd);
        close(fd); // close client fd from server
        establishedClients.erase(it);
        lg(LOG_INFO) << "Client with fd [" << fd << "] was closed." << endlog;
        return true;
    }
}

void Application::handle_pending_request(ClientInfo & client,int fd){
    auto & req = client.pending_request;
    thread_local static HTTPResponse response = []{
        HTTPResponse resp;
        resp.data.emplace();
        resp.version.major = 1;
        resp.version.minor = 1;
        return resp;
    }();
    using M = HTTPRequest::HTTPMethod;

    switch(req.method){
    case M::GET:{
        if(req.headers.find(KEY_Content_Length) != req.headers.end() ||
           req.headers.find(KEY_Transfer_Encoding) != req.headers.end()){ //bad request
            lg(LOG_ERROR) << "Client " << fd << " passed GET request with message body!" << endlog;
            send_message_simp(fd,HTTPResponse::StatusCode::BadRequest,
                HTTPResponse::get_status_description(HTTPResponse::StatusCode::BadRequest));
            cleanup_connection(fd);
            return;
        }else{ //nice guys now we can send something useful back
            response.data->clear();
            response.headers.clear();
            const char * test_msg = "Hello World!";
            response.data->insert(response.data->end(),test_msg,test_msg + strlen(test_msg));
            response.headers["Words"] = "I Love U";
            response.status_code = HTTPResponse::StatusCode::OK;
            response.status_str = "OK";
            response.headers["Content-Type"] = "text/plain; charset=utf-8";
            response.headers["Content-Length"] = std::to_string(response.data->size());
            response.headers["Connection"] = "keep-alive";
            lg(LOG_TRACE) << "Send reply to client " << fd << endlog;
            send_message(fd,response);
        }
        client.pending = false;
        break;
    }
    case M::POST:{ // there's data in the message body,so we need to extract the data
        bool ok = false;
        auto p = req.headers.find(KEY_Transfer_Encoding);
        if(!client.pending_request.data)client.pending_request.data.emplace();
        if(p != req.headers.end()){
            if(p->second.compare("chunked")){
                lg(LOG_ERROR) << "Transfer-Encoding mode "  << p->second <<  " is not supported." << std::endl;
                return;
            }
            // chunked mode
            // read existing data
            while(true){
                std::string_view buffer (client.buffer.begin(),client.buffer.end());
                char * endptr = nullptr;
                size_t pos = buffer.find_first_of("\r\n");
                if(pos == std::string_view::npos)return; // data is not well prepared
                int count = strtol(buffer.data(),&endptr,16); // @note the chunked data is a hex number
                if(endptr == buffer.data()){ // invalid character
                        lg(LOG_ERROR) << "Client " << fd << " passed POST request with bad message body!" << endlog;
                        send_message_simp(fd,HTTPResponse::StatusCode::BadRequest,
                            HTTPResponse::get_status_description(HTTPResponse::StatusCode::BadRequest));
                        cleanup_connection(fd);
                }
                if(((int)(buffer.size()) - 4 - count - pos) >= 0){ // why minus 4? ==> two \r\n
                    if(count)client.pending_request.data->insert(client.pending_request.data->end(),
                            buffer.begin() + (pos+2),buffer.begin() + (pos+2+count));
                    // check whether it ends with \r\n]
                    if(strncmp("\r\n",buffer.data() + (pos+2+count),2)){
                        lg(LOG_ERROR) << "Client " << fd << " passed POST request with bad message body!" << endlog;
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
                    lg(LOG_WARN) << "Data Well Prepared!" << endlog;
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
        // @todo ... add code here
        client.pending = false;
        lg(LOG_INFO) << "Received POST message from " << fd 
                 << " - Method: " << HTTPRequest::getMethodString(client.pending_request.method)
                 << " - URL: " << client.pending_request.url.full_path()
                 << " - Data size: " << (client.pending_request.data ? client.pending_request.data->size() : 0)
                 << " bytes" << endlog;
        response.data->clear();
        response.headers.clear();
        const char * test_msg = "Hello World!";
        response.data->insert(response.data->end(),test_msg,test_msg + strlen(test_msg));
        response.headers["Words"] = "I Love U";
        response.status_code = HTTPResponse::StatusCode::OK;
        response.status_str = "OK";
        response.headers["Content-Type"] = "text/plain; charset=utf-8";
        response.headers["Content-Length"] = std::to_string(response.data->size());
        response.headers["Connection"] = "keep-alive";
        lg(LOG_INFO) << "Send reply to client " << fd << endlog;
        send_message(fd,response);
        break;
    }
    default: //no way!
        break;
    }
}

size_t Application::send_message_simp(int fd,HTTPResponse::StatusCode code,std::string_view stat_str){
    thread_local static HTTPResponse resp;
    resp.version.major = 1;
    resp.version.minor = 1;
    resp.status_code = code;
    resp.status_str.assign(stat_str.begin(),stat_str.end());

    return send_message(fd,resp);
}

size_t Application::send_message(int fd,const HTTPResponse & resp){
    auto data = resp.generate();
    return send(fd,data.data(),data.size(),0);
}