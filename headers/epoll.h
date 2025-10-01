/**
 * @file epoll.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Wrapper of EPOLL Module
 * @version 0.1
 * @date 2025/09/30
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/09/30 
 */
#ifndef MN_EPOLL
#define MN_EPOLL
#include <sys/socket.h>
#include <sys/epoll.h>
#include <vector>
#include <ranges>

namespace mnginx{
    struct EPoll{
        int epoll_fd;
        /** Why here I dont use pmr allocator?
         * 'Cause events seldom enlarge
         */
        std::vector<epoll_event> events;

        // @todo also config
        inline EPoll(){
            epoll_fd = epoll_create1(0);
        }
        
        inline void resize(unsigned int size){
            events.resize(size);
        }

        /// @note for speed reason,no additional check here
        inline auto wait(int timeout_ms){
            int nfds = epoll_wait(epoll_fd,events.data(),events.size(),timeout_ms);
            return std::pair(nfds >= 0,std::views::counted(events.begin(),std::max(nfds,0)));
        }

        inline int add(uint32_t fd,uint32_t care_events){
            epoll_event ev;
            ev.events = care_events;
            ev.data.fd = fd;
            return epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ev);
        }

        inline int del(uint32_t fd){
            return epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,nullptr);
        } 

        inline int mod(uint32_t fd,uint32_t care_events){
            epoll_event ev;
            ev.events = care_events;
            ev.data.fd = fd;
            return epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&ev);
        } 

    };
}

#endif