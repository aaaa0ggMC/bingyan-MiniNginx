/**
 * @file client.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Some data about clients.
 * @version 0.1
 * @date 2025/10/02
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/10/02 
 */
#ifndef MN_CLIENT
#define MN_CLIENT
#include <http_parser.h>

namespace mnginx{
    /**
     * @brief The data held by a mnginx::Server to save client incoming requests and response
     * @start-date 2025/10/02
     */
    struct ClientInfo{
        /// the buffer to store data coming from client
        std::pmr::vector<char> buffer;
        /// the cached value used to reduce the characters need searching
        size_t find_last_pos;
        /// pending request whose message body is incomplete
        /// (useful in POST,but may be improved one day to support large-file transfer)
        HTTPRequest pending_request;
        /// to determine whether pending_request is pending
        bool pending; 
        /// client id
        uint64_t client_id;

        /// current max id to keep client unique
        static uint64_t max_client_id;

        /// constructor
        inline ClientInfo(){
            find_last_pos = 0;
            client_id = ++max_client_id;
            pending = false;
        }
    };
} // namespace mnginx
#endif