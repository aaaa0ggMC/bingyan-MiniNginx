#ifndef MN_CLIENT
#define MN_CLINET
#include <http_parser.h>

namespace mnginx{
    struct ClientInfo{
        std::pmr::vector<char> buffer;
        size_t find_last_pos;
        HTTPRequest pending_request;
        bool pending; 

        inline ClientInfo(){
            find_last_pos = 0;
            pending = false;
        }
    };
} // namespace mnginx


#endif