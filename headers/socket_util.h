/**
 * @file socket_util.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Some useful functions
 * @version 0.1
 * @date 2025/10/05
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/10/05 
 */
#ifndef MN_SOCK_UTIL
#define MN_SOCK_UTIL
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#include <cmath>

// yes,now you can use the go grammar
#define CAT(a,b) a##b
#define CAT_2(a,b) CAT(a,b)
#define defer defer_t CAT_2(defer,__COUNTER__) = [&] noexcept   

namespace mnginx{

    template<class T> struct defer_t{
        T defer_func;
        inline defer_t(T functor):defer_func(std::move(functor)){}
        inline ~defer_t() noexcept{defer_func();}

        defer_t(const defer_t &) = delete;
        defer_t(defer_t &&) = delete;

        defer_t& operator=(const defer_t&) = delete;
        defer_t& operator=(defer_t&&) = delete;
    };

    struct Util{
        /// clamp the backlog count to the system's maximum if requested is too large
        static inline int safe_back_log(int requested){
            return std::min(requested,SOMAXCONN);
        }

        /// create a nonblock tcp socket via one function
        static inline int create_nonblocking_tcp_socket(){
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if(fd == -1)return -1;
            
            int flags = fcntl(fd, F_GETFL, 0);
            if(flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
                close(fd);
                return -1;
            }
            return fd;
        }

        /// set a address to resuable
        inline static bool set_reuse_addr(int fd){
            int reuse = 1;
            return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == 0;
        }

        /// cast ip addr_t to human-readable ip string
        inline static char* ip_to_str(in_addr_t addr){
            thread_local static char client_ip[INET_ADDRSTRLEN];
            memset(client_ip, 0, sizeof(client_ip));
            inet_ntop(AF_INET,&addr,client_ip,sizeof(client_ip));
            return client_ip;
        }

        /// get s_addr according to name
        inline static std::pair<in_addr_t, std::string> get_s_addr_with_error(std::string_view i_str) {
            std::string str (i_str.begin(),i_str.end());
            struct in_addr addr;
            if(inet_pton(AF_INET, str.data(), &addr) == 1){
                return {addr.s_addr, ""};
            }
            
            struct hostent* host = gethostbyname(str.data());
            if(host && host->h_addrtype == AF_INET && host->h_addr_list[0]){
                return {reinterpret_cast<struct in_addr*>(host->h_addr_list[0])->s_addr, ""};
            }
            return {INADDR_NONE, "Failed to resolve: " + std::string(str)};
        }
    };
}

#endif