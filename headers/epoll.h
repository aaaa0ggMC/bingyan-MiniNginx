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

namespace mnginx{
    struct EPoll{
        static int flag;
        int epoll_fd;

        inline EPoll(){
            epoll_fd = epoll_create(++flag);
        }


    };
}

#endif