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
        InvalidMethod,
        InvalidURL
    };

    struct URL{
    private:
        std::pmr::string full_path_;
        std::string_view main_path_;
    public:
        inline const std::pmr::string& full_path(){return full_path_;}
        inline std::string_view main_path(){
            if(full_path_.empty())return "";
            return main_path_;
        }

        int parse_raw_url(std::string_view raw_url);
        std::pmr::unordered_map<std::pmr::string,std::pmr::string> get_args();
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
        URL url;
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

        enum class TransferMode : int32_t{
            ContentLength,
            Chunked
        };

        // make sure clen or chunked is set
        void checkout_data(TransferMode = TransferMode::ContentLength);

        // It's said that the compiler will use RVO to optimize the code,so std::move is not suggested
        std::pmr::vector<char> generate(TransferMode = TransferMode::ContentLength) const; 

        void reset();
    };

}
#endif
