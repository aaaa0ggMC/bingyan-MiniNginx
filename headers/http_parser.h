#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory_resource>

namespace mnginx{
    using HTTPData = std::pmr::vector<char>;
    using HTTPHeaderHashMap = std::pmr::unordered_map<std::pmr::string,std::pmr::string>; 

    /**
     * @brief HTTP Version Control,since this project is a prototype,mininginx uses HTTP/1.1
     * @start-date 2025/09/30
     */
    struct HTTPVersion{
        int major;
        int minor;

        inline HTTPVersion():major{1},minor{1}{}
    };
    
    /**
     * @brief HTTP Requests from client
     * @start-date 2025/09/30
     */
    struct HTTPRequest{
        enum HTTPMethod : int32_t{
            GET,
            POST
        };

        //// Request Line
        HTTPMethod method;
        std::pmr::string url;
        HTTPVersion version;

        //// Header
        HTTPHeaderHashMap headers;

        //// Data
        std::optional<HTTPData> data;


        void parse(std::string_view data);
        std::pmr::vector<char> generate(); 
    };

    struct HTTPResponse{
        //// In this file I stored some response code I used
        /// I use #include to prevent the struct being verbose
        #include "./imp/response_code.h"

        //// Status Line
        HTTPVersion version;
        StatusCode status_code;
        std::pmr::string status_str;

        //// Response Header
        HTTPHeaderHashMap headers;

        /// Data
        std::optional<HTTPData> data;

        void parse(std::string_view data);
        // It's said that the compiler will use RVO to optimize the code,so std::move is not suggested
        std::pmr::vector<char> generate(); 
    };

}
