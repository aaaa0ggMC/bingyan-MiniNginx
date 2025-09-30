#include <http_parser.h>
#include <gtest/gtest.h>
#include <alib-g3/aclock.h>
// #include <format>

using namespace mnginx;

/// @todo WIP
ParseCode HTTPResponse::parse(std::string_view data){
    return ParseCode::Success;
}

/** perf
 * @before 6593.11ms for 1'000'000 times ==> 6.59311us /call
 * @after(current) 3217.43ms for 1'000'000  times ==> 3.21743us /call
 */
std::pmr::vector<char> HTTPResponse::generate(){
    using data_t = std::pmr::vector<char>;
    data_t rdata;
    rdata.reserve(2048);

    // used for small numbers,or frankly speaking,for HTTP Version code
    static auto append_num = [](data_t & vec,int num){
        if(num < 10){
            vec.push_back('0' + num);
        }else if(num < 100){
            vec.push_back('0' + (num/10) % 10);
            vec.push_back('0' + num % 10);
        }else if(num < 1000){
            vec.push_back('0' + num/100 % 10);
            vec.push_back('0' + num/10 % 10);
            vec.push_back('0' + num % 10);
        }else{
            std::cout << "Well,you win" << std::endl;
        }// status code < 1000
    };

    rdata.insert(rdata.end(),{'H','T','T','P','/'});
    append_num(rdata,version.major);
    if(version.minor){
        rdata.push_back('.');
        append_num(rdata,version.major);
    }
    rdata.push_back(' ');
    append_num(rdata,static_cast<int32_t>(status_code));
    rdata.push_back(' ');
    rdata.insert(rdata.end(),status_str.begin(),status_str.end());
    rdata.insert(rdata.end(),{'\r','\n'});

    for(auto&[key,val] : headers){
        rdata.insert(rdata.end(),key.begin(),key.end());
        rdata.insert(rdata.end(),{':',' '});
        rdata.insert(rdata.end(),val.begin(),val.end());
        rdata.insert(rdata.end(),{'\r','\n'});
    }
    rdata.insert(rdata.end(),{'\r','\n'});
    
    if(!!data){
        HTTPData & d = *data;
        rdata.insert(rdata.end(),d.begin(),d.end());
    }
    return rdata;
}

// @todo 请求的网页可能带非法字符比如空格，因此有%20的转换！
ParseCode HTTPRequest::parse(std::string_view str){
    auto pos = str.find_first_of(' ');
    auto oldpos = 0;
    //// Extract Method
    if(pos == std::string_view::npos)return ParseCode::InvalidFormat;
    method = getMethod(str.substr(0,pos));

    //// Extract URL
    oldpos = pos + 1;
    pos = str.substr(oldpos).find_first_of(' ') + oldpos;
    if(pos == std::string_view::npos)return ParseCode::InvalidFormat;
    auto str_part = str.substr(oldpos,pos - oldpos);
    url.reserve(str_part.size()+1);
    url.clear();
    url.insert(url.end(),str_part.begin(),str_part.end());

    //// Extract HTTP Version
    oldpos = pos+1;
    if(strncmp(&(str[oldpos]),"HTTP/",5))return ParseCode::InvalidFormat;
    {
        char * endptr = nullptr;
        const char * startptr = &str[oldpos + 5];
        //// Major
        long result = strtol(startptr,&endptr,10);
        if(endptr == startptr){
            // cast error,return invalid fmt
            return ParseCode::InvalidFormat;
        }
        version.major = result;
        //// Minor
        if(*endptr == '.'){// We have a minor version here...
            startptr = endptr + 1;
            result = strtol(startptr,&endptr,10);
            if(startptr == endptr)return ParseCode::InvalidFormat;
            version.minor = result;
        }
        pos = endptr - str.data(); // normally,*endptr == '\r' now
        if(*endptr != '\r')return ParseCode::InvalidFormat; // SUS
    }

    auto is_space = [](char ch){
        return ch == ' ' || ch == '\t';
    };

    auto is_valid_header_key_char = [](char c){
        if (std::isalnum(c)) {
            return true;
        }
        switch (c) {
            case '!': case '#': case '$': case '%': case '&': case '\'':
            case '*': case '+': case '-': case '.': case '^': case '_':
            case '`': case '|': case '~':
                return true;
            default:
                return false;
        }
    };

    //// Go Down To The Next Line And Extract Headers
    do{
        pos += 2; //skip \r\n to next line
        oldpos = pos;
        // actually,here we call pos-->count
        pos = str.substr(oldpos).find_first_of("\r\n");
        if(pos == std::string_view::npos)return ParseCode::InvalidFormat; // So,there's no ending
        else if(pos == 0) {
            pos = oldpos; // keep the right value
            break; // We reached to the end line
        }

        // Normal Line
        std::string_view full_line = str.substr(oldpos,pos);
        pos += oldpos;
        if(is_space(full_line[0]))return ParseCode::SpaceInTheFrontOfHeader; // "  Data: Host" is forbidden
        auto colon_pos = full_line.find_first_of(":");
        if(colon_pos == std::string_view::npos) return ParseCode::InvalidFormat; // "      \r\n" is forbidden
        std::string_view key = full_line.substr(0,colon_pos),
                        value = full_line.substr(colon_pos+1);
        std::pmr::string p_key (key.begin(),key.end());
        // Check if key is valid
        for(char ch : p_key){
            if(!is_valid_header_key_char(ch))return ParseCode::InvalidKeyName;
        }
        // skip spaces
        int i = 0;
        for(;is_space(value[i]);++i){
            // @todo make this configurable
            if(i > 8){
                return ParseCode::TooManySpaces;
            }
        }

        // @todo add more detects for emplace,use try_emplace
        headers.emplace(
            std::move(p_key),
            std::pmr::string(value.begin() + i,value.end())
        );
        //std::cout << "[" << p_key << "]:[" << p_value << "]" << std::endl;
    }while(true);
    //// The Rest Of The Content
    pos += 2;
    data.emplace();
    // @todo need to process data with Content-Length or Transfer-Encoding
    data->insert(data->end(),str.begin() + pos,str.end());
    return ParseCode::Success;
}

/// @todo WIP
std::pmr::vector<char> HTTPRequest::generate(){
    std::pmr::vector<char> rdata;
    return rdata;
}


HTTPRequest::HTTPMethod HTTPRequest::getMethod(std::string_view str){
    using M = HTTPRequest::HTTPMethod;
    int str_size = str.size();
    //discard some useless data
    if(str_size <= 2)return M::InvalidMethod;
    
    if(!strncmp(str.data(),"GET",3))return M::GET;
    else if(!strncmp(str.data(),"POST",4))return M::POST;
    else return M::InvalidMethod;
}

std::string_view HTTPRequest::getMethodString(HTTPRequest::HTTPMethod method){
    using M = HTTPRequest::HTTPMethod;

    switch(method){
    case M::GET:
        return "GET";
    case M::POST:
        return "POST";
    default:
        return "INVALID";
    }
}


//// TESTS ////
#ifdef MNGINX_TESTING
TEST(HTTPResponse, generate){
    mnginx::HTTPResponse resp;
    resp.version.major = 1;
    resp.version.minor = 1;  // HTTP/1.1
    resp.status_code = HTTPResponse::StatusCode::OK;
    resp.status_str = "OK";
    resp.headers["Content-Type"] = "text/html";
    mnginx::HTTPData test_data{'H', 'e', 'l', 'l', 'o'};
    resp.headers["Content-Length"] = std::to_string(test_data.size());

    resp.data = test_data;
    auto generated_response = resp.generate();

    EXPECT_FALSE(generated_response.empty());
    std::string response_str(generated_response.begin(), generated_response.end());
    std::cout << alib::g3::Util::str_unescape(response_str) << std::endl;
    EXPECT_TRUE(response_str.find("HTTP/1.1 200 OK") != std::string::npos);
    EXPECT_TRUE(response_str.find("Content-Type: text/html") != std::string::npos);
    EXPECT_TRUE(response_str.find("Content-Length: ") != std::string::npos);
    if(resp.data){
        EXPECT_TRUE(response_str.find("Hello") != std::string::npos);
    }
    
    /*
    std::cout << "Let's generate http request for 1,000,000 times!" << std::endl;
    alib::g3::Clock clk;
    for(int i = 0;i < 1'000'000;++i){
        resp.generate();
    }
    std::cout << "It costs " << clk.getOffset() << "ms" << std::endl;
    */
}

/* This test result varies in differect architecture,so I commented it.
TEST(HTTPResponse, generateFullIntegrity) {
    HTTPResponse resp;
    resp.version.major = 1;
    resp.version.minor = 1;  // HTTP/1.1
    resp.status_code = HTTPResponse::StatusCode::OK;
    resp.status_str = "OK";
    HTTPData test_data{'H', 'e', 'l', 'l', 'o'};
    resp.headers["Content-Type"] = "text/html";
    resp.headers["Content-Length"] = std::to_string(test_data.size());
    resp.data = test_data;
    auto generated_response = resp.generate();
    std::string expected_response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "Hello";
    std::string response_str(generated_response.begin(), generated_response.end());

    EXPECT_EQ(response_str, expected_response);
    EXPECT_EQ(response_str.length(), expected_response.length());
}*/

#endif