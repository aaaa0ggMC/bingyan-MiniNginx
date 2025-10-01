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
                lg(LOG_WARN) << "Waited to timeout but received no clients!" << endlog;
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

    // @todo remember to add while(1) and write it into buffer
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
    }

    if(shouldClose){
        cleanup_connection(client_fd);
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