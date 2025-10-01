/**
 * @file http_parser.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief Simple HTTP Parser
 * @version 0.1
 * @date 2025/09/30
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/09/30 
 */
#ifndef MN_PARSER
#define MN_PARSER
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory_resource>

namespace mnginx{
    using HTTPData = std::pmr::vector<char>;
    using HTTPHeaderHashMap = std::pmr::unordered_map<std::pmr::string,std::pmr::string>; 

    constexpr const char * KEY_Content_Length = "Content-Length";
    constexpr const char * KEY_Transfer_Encoding = "Transfer-Encoding";

    /**
     * @brief HTTP Version Control,since this project is a prototype,mininginx uses HTTP/1.1
     * @start-date 2025/09/30
     */
    struct HTTPVersion{
        int major;
        int minor;

        inline HTTPVersion():major{1},minor{1}{}
    };
    
    enum class ParseCode : int32_t{
        Success,
        InvalidFormat,
        TooManySpaces,
        SpaceInTheFrontOfHeader,
        InvalidKeyName,
        InvalidMethod
    };

    /**
     * @brief HTTP Requests from client
     * @start-date 2025/09/30
     */
    struct HTTPRequest{
        enum class HTTPMethod : int32_t{
            GET,
            POST,
            InvalidMethod
        };

        //// Request Line
        HTTPMethod method;
        std::pmr::string url;
        HTTPVersion version;

        //// Header
        HTTPHeaderHashMap headers;

        //// Data
        std::optional<HTTPData> data;


        ParseCode parse(std::string_view data);
        std::pmr::vector<char> generate();

        static HTTPMethod getMethod(std::string_view str);
        static std::string_view getMethodString(HTTPMethod method);
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

        ParseCode parse(std::string_view data);
        // It's said that the compiler will use RVO to optimize the code,so std::move is not suggested
        std::pmr::vector<char> generate() const; 
    };

}
#endif
