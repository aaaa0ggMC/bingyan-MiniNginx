#ifndef MN_MOD_RP
#define MN_MOD_RP
#include <http_parser.h>
#include <route_states.h>
#include <client.h>
#include <alib-g3/alogger.h>
#include <epoll.h>

namespace mnginx::modules{
    struct ReverseClient{
        int client_fd;

        inline ReverseClient(){
            client_fd = -1;
        }

        inline ~ReverseClient(){
            if(client_fd != -1)close(client_fd);
        }

        void setup();
    };

    template<class T> concept HasClientId = requires(T & t){
        {t.client_id} -> std::convertible_to<uint64_t>;
    };

    struct ModReverseProxy{
        static thread_local ModReverseProxy proxy;
        std::pmr::unordered_map<uint64_t,ReverseClient> clients;
        std::pmr::unordered_map<int,ClientInfo> * server_clients;
        alib::g3::LogFactory * lg;

        inline static void module_init(int server_fd,
                        EPoll& epoll,
                        std::pmr::unordered_map<int,ClientInfo>& cl,
                        alib::g3::LogFactory & lg_acc,
                        alib::g3::LogFactory & lg_err){
            proxy.server_clients = &cl;
            proxy.lg = & lg_acc;
            lg_acc(LOG_INFO) << "I initialized." << std::endl;
        }

        inline static void module_timer(double elap_ms){
            
        }

        inline ModReverseProxy(){
            server_clients = nullptr;
        }

        // we only care about keys
        inline void clear_unused_connections(){
            if(!server_clients)return;
            auto cl = server_clients;
            std::erase_if(clients,[cl](const auto & item){
                auto & [k,_] = item;
                return (cl->find(k) == cl->end());
            });
        }

        static HandleResult handle(HandlerContext ctx);
    };
}
#endif