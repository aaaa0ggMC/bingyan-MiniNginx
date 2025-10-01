/**
 * @file application.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief The Main Procedure
 * @version 0.1
 * @date 2025/09/30
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/09/30 
 */
#ifndef MN_APP
#define MN_APP 
#include <alib-g3/alogger.h>
#include <http_parser.h>
#include <memory_resource>
#include <epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Since my library is verbose,so here I globally use the namespace
using namespace alib::g3;

namespace mnginx{
    struct ClientInfo{
<<<<<<< HEAD

=======
        std::pmr::vector<char> buffer;
        size_t find_last_pos;
        HTTPRequest pending_request;
        bool pending; 

        inline ClientInfo(){
            find_last_pos = 0;
            pending = false;
        }
>>>>>>> dev
    };

    class Application{
    private:
        //// Log System
        Logger logger;
        LogFactory lg;
        
        //// Memory Management
        std::pmr::synchronized_pool_resource pool;
        std::pmr::memory_resource * resource;

        int server_fd;
        sockaddr_in address;
        EPoll epoll;

        std::pmr::unordered_map<int,ClientInfo> establishedClients; 
    public:
        int return_result;

        Application();
        ~Application();

        //// Setup Section ////
        void setup();
        void setup_general();
        void setup_server();
        void setup_logger();

        //// sub procedures ////
        void accept_connections();
        void handle_client(epoll_event & ev);
        bool cleanup_connection(int fd);
<<<<<<< HEAD
=======
        void handle_pending_request(ClientInfo & client,int fd);
        /// note that code must lower than 1000
        size_t send_message_simp(int fd,HTTPResponse::StatusCode code,std::string_view stat_str);
        size_t send_message(int fd,const HTTPResponse & resp);
>>>>>>> dev

        //// Main Section ////
        void run();
    };

}

#endif