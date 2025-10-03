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
    //// Some abbr. types
    using HTTPData = std::pmr::vector<char>;
    using HTTPHeaderHashMap = std::pmr::unordered_map<std::pmr::string,std::pmr::string>; 
    //// Some keys that are essential in HTTP contacts
    /// The transfer mode of the message body.Has higher priority than Content-Length(See code below)
    constexpr const char * KEY_Transfer_Encoding = "Transfer-Encoding";
    /// The content length of the message body
    constexpr const char * KEY_Content_Length = "Content-Length";

    /// type of the return of HTTPRequest/HTTPResponse::parse
    enum class ParseCode : int32_t{
        /// everything works just fine
        Success, 
        /// the http message is not nice(from many aspects),will be divided into more detailed code later
        InvalidFormat,
        /// too many spaces(over 32 between key and value) are passed for the parser to "skip",
        /// so the parser treats you a SUS connection
        TooManySpaces,
        /// any space in the front of header key is forbidden: "A: B"(nice) " A: B"(nope)
        SpaceInTheFrontOfHeader,
        /// key name has characters out of alphabets,numbers and  !#$%&*+-.^_`|~
        InvalidKeyName,
        /// Invalid HTTP Method,currently only GET and POST is supported
        InvalidMethod,
        /// a url is not good that makes URL::parse_raw_url crashed
        InvalidURL
    };

    /**
     * @brief HTTP Version Control,since this project is a prototype,mininginx uses HTTP/1.1
     * @start-date 2025/09/30
     */
    struct HTTPVersion{
        /// The major version,default 1
        int major;
        /// The minor version,default 1
        int minor;

        /// constructor that builds the default value 1.1
        inline HTTPVersion(int _major = 1,int _minor = 1):major{_major},minor{_minor}{}
    };

    /**
     * @brief The exact url extracted from raw url sent by client
     * @start-date 2025/10/02
     */
    struct URL{
    private:
        /// full decoded path of url
        std::pmr::string full_path_;
        /// decoded url that discarded args ==> "abc?a=1"->"abc", "anc%3F?a=2"->"anc?"
        std::string_view main_path_;
        /// used for archive 
        std::pmr::string raw_path_;
    public:
        /// get the const version of full_path_ in case of accidental changes that destroys main_path_
        inline const std::pmr::string& full_path(){return full_path_;}
        /// get main path
        inline std::string_view main_path(){
            if(full_path_.empty())return "";
            return main_path_;
        }
        /// get raw path
        inline const std::pmr::string& raw_path(){return raw_path_;}

        /// @brief parse the raw url to decoded url 
        /// @return currently,it always returns 0
        int parse_raw_url(std::string_view raw_url);

        /// @brief (@WIP,NotImplemented),dynamically generate arg list for some scenarios
        /// @return the arg list
        std::pmr::unordered_map<std::pmr::string,std::pmr::string> get_args();
    };

    /**
     * @brief HTTP Requests from client
     * @start-date 2025/09/30
     */
    struct HTTPRequest{
        /// Supported Methods
        enum class HTTPMethod : int32_t{
            GET, ///< HTTP GET
            POST, ///< HTTP POST
            InvalidMethod ///< All Other unsupported types are categorized to this
        };

        //// Request Line
        /// HTTP Method
        HTTPMethod method;
        /// Decoded URL
        URL url;
        /// The HTTP version client used
        HTTPVersion version;

        //// Header
        /// headers(k-v pairs) of the client
        HTTPHeaderHashMap headers;

        //// Data
        /// message body of the request
        std::optional<HTTPData> data;

        /**
         * @brief Parse message(only process message header) 
         * 
         * @param data the header line,
         * must end with "\r\n\r\n",
         * or the function will return InvalidFormat
         * (However the data is still usable)
         * @return ParseCode parse result
         * @start-date 2025/10/02
         */
        ParseCode parse(std::string_view data);

        /**
         * @brief (WIP,NotImplemented) generate a HTTP Request According to the data
         * 
         * @return std::pmr::vector<char> 
         * @start-date 2025/10/02
         */
        std::pmr::vector<char> generate();

        /**
         * @brief (WIP,NotImplemented) generate a HTTP Request According to the data
         * @start-date 2025/10/02
         */
        void generate_to(std::pmr::vector<char> & cont);

        /**
         * @brief Get the Method object
         * @param str 
         * @return HTTPMethod 
         * @start-date 2025/10/02
         */
        static HTTPMethod getMethod(std::string_view str);
        /**
         * @brief Get the Method String object
         * @param method 
         * @return std::string_view 
         * @start-date 2025/10/02
         */
        static std::string_view getMethodString(HTTPMethod method);
    };

    /**
     * @struct HTTPResponse
     * @brief Represents an HTTP response, including status line, headers, and body data.
     * @start-date 2025/10/02
     */
    struct HTTPResponse{
        //// In this file I stored some response code I used
        //// I use #include to prevent the struct being verbose
        #include "./imp/response_code.h"

        /**
         * @brief Data transfer mode used by checkout_data
         * @start-date 2025/10/02
         */
        enum class TransferMode : int32_t{
            ContentLength, ///< use Content-Length
            Chunked ///< use Transfer-Encoding: chunked
        };

        //// Status Line
        /// HTTP version used to send data,by default 1.1
        HTTPVersion version;
        /// the status code of the server
        StatusCode status_code;
        /// a brief string version of status code
        std::pmr::string status_str;

        /// response headers
        HTTPHeaderHashMap headers;

        /// Data
        std::optional<HTTPData> data;

        /**
         * @brief (WIP,NotImplemented)parse data to response struct
         * @param data 
         * @return ParseCode 
         * @start-date 2025/10/02
         */
        ParseCode parse(std::string_view data);

        // make sure clen or chunked is set
        /**
         * @brief make sure the size and the layout of message body is right
         * will be called in Server::send_message before data is sent 
         * @start-date 2025/10/02
         */
        void checkout_data(TransferMode = TransferMode::ContentLength);

        // It's said that the compiler will use RVO to optimize the code,so std::move is not suggested
        /**
         * @brief generate data to the be sent to client
         * @return std::pmr::vector<char> data
         * @start-date 2025/10/02
         */
        std::pmr::vector<char> generate() const; 

        /**
         * @brief generate data to the be sent to client
         * @start-date 2025/10/02
         */
        void generate_to(std::pmr::vector<char>& cont) const; 

        /**
         * @brief reset members to default value
         * @note if data is not std::nullopt,the ref will be kept and we call data->clear
         * @start-date 2025/10/02
         */
        void reset();
    };

}
#endif
