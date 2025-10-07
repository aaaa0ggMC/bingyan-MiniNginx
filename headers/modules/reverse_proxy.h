/**
 * @file reverse_proxy.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief ReverseProxy module of miningnix
 * @version 0.1
 * @date 2025/10/04
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/10/04 
 */
#ifndef MN_MOD_RP
#define MN_MOD_RP
#include <http_parser.h>
#include <route_states.h>
#include <client.h>
#include <alib-g3/alogger.h>
#include <epoll.h>
#include <arpa/inet.h>

namespace mnginx::modules{
<<<<<<< Updated upstream
    constexpr int mode_reverse_proxy_buffer_size = 1024;
=======
    constexpr int mod_reverse_proxy_buffer_size = 1024;
    constexpr double mod_reverse_proxy_elapse_time = 10;
>>>>>>> Stashed changes
    
    struct ReverseClientConfig{
        sockaddr_in target;
        
        bool modified;

        void reset(){
            modified = false;
            target.sin_family = AF_INET;
            target.sin_port = htons(7000);
            inet_pton(AF_INET,"127.0.0.1",&target.sin_addr);
        }

        inline void modify(){
            modified = true;
        }

        inline ReverseClientConfig(){
            reset();
        }
    };

    /**
     * @brief clients held by module to connect the final server
     * @start-date 2025/10/04
     */
    struct ReverseClient{

        int client_fd; ///< the connection fd between final server and foward sever
        std::pmr::vector<char> buffer; ///< the cached buffer between final server and forward server
        bool connected; ///< check whether reverse client is connected
        int reverse_fd; ///< the client to send
        ReverseClientConfig config; ///< reverse client config

        /// constructor,receives the fd to send proxied data to
        inline ReverseClient(int fd){
            client_fd = -1;
            connected = false;
            reverse_fd = fd;
        }

        /// close the connection
        inline ~ReverseClient(){
            if(client_fd != -1)close(client_fd);
        }

        /// create a new connection socket with the server
        int setup();
    
        /// connect to the server,will call setup() first
        int connect_server();

        /// send data to server
        int send_data(const std::pmr::vector<char> & data);

        /// recieve data from server and proxy it
        HandleResult reverse_proxy();
    };

    template<class T> concept HasClientId = requires(T & t){
        {t.client_id} -> std::convertible_to<uint64_t>;
    };

    /**
     * @brief The reverse proxy module
     * 
     * @start-date 2025/10/04
     */
    struct ModReverseProxy{
        /// per-thread data
        static thread_local ModReverseProxy proxy;

        //// some essential memebers ////
        /// current connections
        std::pmr::unordered_map<uint64_t,ReverseClient> clients;
        /// the connected clients of Server,used to remove unavailable reverse targets
        std::pmr::unordered_map<int,ClientInfo> * server_clients;
        /// logger
        alib::g3::LogFactory * lg;
        /// logger_err
        alib::g3::LogFactory * lge;

        /// init module,must be static and use variable "proxy" to access data
        inline static void module_init(int server_fd,
                        EPoll& epoll,
                        std::pmr::unordered_map<int,ClientInfo>& cl,
                        alib::g3::LogFactory & lg_acc,
                        alib::g3::LogFactory & lg_err){
            proxy.server_clients = &cl;
            proxy.lg = & lg_acc;
            proxy.lge = & lg_err;
            lg_acc(LOG_INFO) << "Module ReverseProxy Inited" << std::endl;
        }

        /// module timer,will be called at fixed rate,must be static
        inline static void module_timer(double elap_ms){
            proxy.clear_unused_connections();
            for(auto & [k,v] : proxy.clients){
                v.reverse_proxy();
            }
        }

        /// constructor
        inline ModReverseProxy(){
            server_clients = nullptr;
        }

        // we only care about keys
        /// clear unused connections according to clients held by mnginx::Server
        inline void clear_unused_connections(){
            if(!server_clients)return;
            auto cl = server_clients;
            std::vector<uint64_t> ids;
            ids.reserve(server_clients->size());
            for(auto & [_,ci] : *cl){
                ids.push_back(ci.client_id);
            }
            std::erase_if(clients,[ids](const auto & item){
                auto & [k,_] = item;
                bool val = std::find(ids.begin(),ids.end(),k) == ids.end();
                return val;
            });
        }

        /// Main function,will be called when route matches
        static HandleResult handle(HandlerContext ctx,const ReverseClientConfig & cfg);
    };
}
#endif