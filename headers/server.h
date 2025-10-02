#ifndef MN_SERVER
#define MN_SERVER
#include <http_parser.h>
#include <route_states.h>
#include <arpa/inet.h>
#include <epoll.h>
#include <client.h>
#include <alib-g3/alogger.h>

namespace mnginx{
    struct Server{
        int server_fd;
        sockaddr_in address;
        EPoll epoll;
        std::pmr::unordered_map<int,ClientInfo> establishedClients; 
    
        alib::g3::LogFactory & lg_acc;
        alib::g3::LogFactory & lg_err;
        alib::g3::LogFactory & lg;

        StateTree & handlers;

        inline Server(
            alib::g3::LogFactory & acc,
            alib::g3::LogFactory & err,
            StateTree & hs
        ):
        lg_acc{acc},lg_err{err},lg{acc},handlers{hs}{ // lg is used for compatitable reason

        }

        ~Server();

        void setup();

        void accept_connections();
        void handle_client(epoll_event & ev);
        bool cleanup_connection(int fd);
        void handle_pending_request(ClientInfo & client,int fd);

        /// note that code must lower than 1000
        size_t send_message_simp(int fd,HTTPResponse::StatusCode code,std::string_view stat_str);
        size_t send_message(int fd,HTTPResponse & resp,HTTPResponse::TransferMode = HTTPResponse::TransferMode::ContentLength);
    };
} // namespace mnginx


#endif