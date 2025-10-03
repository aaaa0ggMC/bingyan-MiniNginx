/**
 * @file server.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief The server of mini-nginx
 * @version 0.1
 * @date 2025/10/03
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/10/03 
 */
#ifndef MN_SERVER
#define MN_SERVER
#include <http_parser.h>
#include <route_states.h>
#include <modules/module_base.h>
#include <arpa/inet.h>
#include <epoll.h>
#include <client.h>
#include <alib-g3/alogger.h>

namespace mnginx{
    /**
     * @brief Server class
     * @start-date 2025/10/03
     */
    struct Server{
        /// file descriptor of current server,it will be inited in setup()
        int server_fd;
        /// address of the server
        /// @todo make this configurable
        sockaddr_in address;
        /// the epoll object
        EPoll epoll;
        /// client established with current server,int is the fd of client
        std::pmr::unordered_map<int,ClientInfo> establishedClients;
        /// module info
        modules::ModuleFuncs mods;
    
        /// access log factory
        alib::g3::LogFactory & lg_acc;
        /// error log factory
        alib::g3::LogFactory & lg_err;
        /// access log factory for compatible reason
        alib::g3::LogFactory & lg;

        /// reference to route the state machine
        StateTree & handlers;

        /// create the framework of the server with logger and state machine
        inline Server(
            alib::g3::LogFactory & acc,
            alib::g3::LogFactory & err,
            StateTree & hs,
            modules::ModuleFuncs & a_mods
        ):
        lg_acc{acc},lg_err{err},lg{acc},handlers{hs},mods{a_mods}{ // lg is used for compatible reason
            server_fd = -1;
        }

        /// destructor that automatically closes the server
        /// note that it is not suggested that you close the server
        /// or the server will be double closed and it may invoke some issues
        inline ~Server(){
            if(server_fd != -1){
                close(server_fd);
            }
        }

        /// setup serverfd and epoll
        void setup();
        /// run the server
        void run();

        /// accept all queued connections when epoll detected EPOLLIN of serverfd
        void accept_connections();
        /// handle client events
        void handle_client(epoll_event & ev);
        /// close and clean the connection with a client 
        bool cleanup_connection(int fd);
        /// handle requests
        void handle_pending_request(ClientInfo & client,int fd);

        /// send code and status string to the client
        /// note that code must lower than 1000
        size_t send_message_simp(int fd,HTTPResponse::StatusCode code,std::string_view stat_str);
        /// send a http response to the client
        size_t send_message(int fd,HTTPResponse & resp,HTTPResponse::TransferMode = HTTPResponse::TransferMode::ContentLength);
    };
} // namespace mnginx
#endif