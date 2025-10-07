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
#include <unistd.h>

namespace mnginx{
    /**
     * @brief A simple epoll wrapper.
     * Use inline for nearly ZERO-OVERHEAD abstraction.
     * @start-date 2025/10/02
     */
    struct EPoll{
        /// file descriptor of epoll object
        int epoll_fd;
        /** Why here I dont use pmr allocator?
         * 'Cause events seldom enlarge
         */
        /// the event list used for epoll_wait to modify
        std::vector<epoll_event> events;

        /// constructor that create an epoll object with flag 0
        inline EPoll(){
            epoll_fd = epoll_create1(0);
        }

        /// destructor that closes the epoll object
        inline ~EPoll(){
            close(epoll_fd);
        }
        
        /// resize the event list
        inline void resize(unsigned int size){
            events.resize(size);
        }

        /// @note wait for events,for speed reason,no additional check here
        /// @param timeout_ms the timeout value, unit: milliseconds
        inline auto wait(int timeout_ms){
            int nfds = epoll_wait(epoll_fd,events.data(),events.size(),timeout_ms);
            return std::pair(nfds >= 0,std::views::counted(events.begin(),std::max(nfds,0)));
        }

        /**
         * @brief add new fd to the epoll pool
         * 
         * @param fd file descriptor of the object you want to listen
         * @param care_events the events you cared
         * @return int 0:success,-1:errors(additional info is stored in errno)
         * @start-date 2025/10/02
         */
        inline int add(uint32_t fd,uint32_t care_events){
            epoll_event ev;
            ev.events = care_events;
            ev.data.fd = fd;
            return epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ev);
        }

        /**
         * @brief delete fd in the epoll pool
         * 
         * @param fd file descriptor of the object you want to delete
         * @return int 0:success,-1:errors(additional info is stored in errno)
         * @start-date 2025/10/02
         */
        inline int del(uint32_t fd){
            return epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,nullptr);
        } 

        /**
         * @brief modify fd events in the epoll pool
         * 
         * @param fd file descriptor of the object you want to modify
         * @param care_events modified events to be listened for 
         * @return int 0:success,-1:errors(additional info is stored in errno)
         * @start-date 2025/10/02
         */
        inline int mod(uint32_t fd,uint32_t care_events){
            epoll_event ev;
            ev.events = care_events;
            ev.data.fd = fd;
            return epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&ev);
        } 

    };
}

#endif