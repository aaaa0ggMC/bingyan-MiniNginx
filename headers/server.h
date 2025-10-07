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
#include <thread>

constexpr size_t mnginx_handle_buffer_size = 4096;
constexpr size_t mnginx_ping_time_ms = 16000; // 16s per ping
constexpr std::string_view ping_data = "HEAD /_keepalive HTTP/1.1\r\nHost: mininginx\r\n\r\n";

namespace mnginx{
    /**
     * @brief Server configs
     * 
     * @start-date 2025/10/05
     */
    struct ServerConfig{
        /// where the server binds to
        sockaddr_in address; 

        /// backlog count used in listen()
        int backlog_count; 
        /// the event list size of epoll
        size_t epoll_event_list_size; 
        /// when calling epoll_wait,this is the timeout value,uint: milliseconds
        /// -1 means suspend till events come
        int epoll_wait_interval_ms; 
        /// to prevent module_timer running so frequently,this is the minimum time timer need to wait
        /// the actual time varies 'cause epoll_wait's return is not predictable
        size_t module_timer_at_least_wait_ms;
        /// HTTP Package Max Size, 0 for no restrictions
        size_t http_package_max_size;
        /// worker count
        size_t workers = 0;

        inline void reset(){
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(9191); // this is not an ad!
            backlog_count = 1024;
            epoll_event_list_size = 1024;
            epoll_wait_interval_ms = 10;
            module_timer_at_least_wait_ms = 5;
            http_package_max_size = 8 * 1024 * 1024;
            workers = 1;
        }

        inline ServerConfig(){
            reset();
        }
    };
    /**
     * @brief Server class
     * @start-date 2025/10/03
     */
    struct Server{
        /// server id
        static int64_t server_id_max;
        /// lock for accept
        static std::mutex accept_lock;

        /// file descriptor of current server,it will be inited in setup()
        int server_fd;
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

        /// reference to route the state machine
        StateTree & handlers;

        /// configs
        ServerConfig config;
        
        /// bool running
        bool running;

        /// current server id
        int64_t server_id;

        /// check clients valid
        alib::g3::Clock clk;
        alib::g3::Trigger ping_trigger;

        /// create the framework of the server with logger and state machine
        inline Server(
            alib::g3::LogFactory & acc,
            alib::g3::LogFactory & err,
            StateTree & hs,
            modules::ModuleFuncs & a_mods
        ):
        lg_acc{acc},lg_err{err},handlers{hs},mods{a_mods},clk(false),
            ping_trigger(clk,mnginx_ping_time_ms){ // lg is used for compatible reason
            server_fd = -1;
            running = true;
            // this happens on the main thread,it's safe
            server_id = server_id_max++;
        }

        /// destructor that automatically closes the server
        /// note that it is not suggested that you close the server
        /// or the server will be double closed and it may invoke some issues
        inline ~Server(){
            using namespace alib::g3;
            if(server_fd != -1){
                close(server_fd);
            }
            lg_acc(LOG_INFO) << "Server#" << server_id << " shutdown" << endlog;
        }

        /// setup serverfd and epoll
        void setup();
        /// run the server
        void run();
        /// close the server
        void close_server();
        /// stop server
        void stop_server();

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
        ssize_t send_message_simp(int fd,HTTPResponse::StatusCode code,std::string_view stat_str);
        /// send a http response to the client
        ssize_t send_message(int fd,HTTPResponse & resp,HTTPResponse::TransferMode = HTTPResponse::TransferMode::ContentLength);
    };
} // namespace mnginx
#endif