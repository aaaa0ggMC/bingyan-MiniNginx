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
std::pmr::vector<char> HTTPResponse::generate() const{
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
    
    if(!!data && data->size() != 0){
        rdata.insert(rdata.end(),data->begin(),data->end());
    }
    return rdata;
}

// 请求的网页可能带非法字符比如空格，因此有%20的转换！(I've done this)
/* perf: 2028.07ms for 1'000'000 calls   ==> 2.02807us / call*/
ParseCode HTTPRequest::parse(std::string_view str){
    auto pos = str.find_first_of(' ');
    auto oldpos = 0;
    //// Extract Method
    if(pos == std::string_view::npos)return ParseCode::InvalidFormat;
    method = getMethod(str.substr(0,pos));
    if(method == HTTPMethod::InvalidMethod){
        return ParseCode::InvalidMethod;
    }

    //// Extract URL
    oldpos = pos + 1;
    pos = str.substr(oldpos).find_first_of(' ') + oldpos;
    if(pos == std::string_view::npos)return ParseCode::InvalidFormat;
    auto str_part = str.substr(oldpos,pos - oldpos);
    if(url.parse_raw_url(str_part) != 0){
        return ParseCode::InvalidURL;
    }

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
    if((str.begin() + (pos+2)) != str.end())return ParseCode::InvalidFormat;
    // parse function doesnt deal with message body
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

//// URL ////
int URL::parse_raw_url(std::string_view raw){
    static auto acceptable = [](long ch){
        if(ch == 0x09 || ch == 0x0A || ch == 0x0D)return true;
        else if(ch <= 0x1F || ch == 0x7F || ch > 0xFF)return false;
        else return true;
    };
    int64_t pos = (int64_t)raw.find_first_of("?");
    if(pos == std::string_view::npos)pos = -1;
    int64_t original = pos;

    // parse characters and find the first "?"
    full_path_.clear();
    full_path_.reserve(raw.size()+1);
    for(size_t i = 0;i < raw.size();++i){
        if(raw[i] == '%'){
            if((int)raw.size()-(int)i < 2)break;
            else{
                char ch[3] = {0};
                char * endptr;
                ch[0] = raw[++i];
                ch[1] = raw[++i];

                long result = strtol(ch,&endptr,16);
                if(!acceptable(result) || endptr != &(ch[2])){
                    // 3 ch is not added
                    if(i <= original)pos -= (endptr - ch) - 1;
                    i -= (&(ch[2]) - endptr);
                    continue;
                } // which means this is an invalid number
                full_path_.push_back((char)result);
                // 2ch is not added
                if(i <= original)pos -= 2;
            }
        }else if(raw[i] == '+')full_path_.push_back(' ');
        else full_path_.push_back(raw[i]);
    }

    // generate main view
    // test if(pos >= 0)std::cout << "Charch:" <<  full_path_[pos] << std::endl;
    if(pos >= 0)main_path_ = std::string_view(full_path_.begin(),full_path_.begin() + pos);
    else main_path_ = std::string_view(full_path_.begin(),full_path_.end());

    return 0;
}

std::pmr::unordered_map<std::pmr::string,std::pmr::string> URL::get_args(){
    // @todo WIP
    return {};
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

TEST(HTTPRequest,parse){
    HTTPRequest req;
    const char * data = "GET / HTTP/1.1\r\n"
                     "Host: example.com\r\n"
                     "User-Agent: MyClient/1.0\r\n"
                     "Accept: */*\r\n"
                     "\r\n";
    ParseCode result = req.parse(data);
    EXPECT_TRUE(req.method == HTTPRequest::HTTPMethod::GET);
    EXPECT_FALSE(req.url.full_path().compare("/"));
    EXPECT_TRUE(req.headers.size() == 3);
    EXPECT_FALSE(req.headers["Host"].compare("example.com"));
    EXPECT_FALSE(req.headers["User-Agent"].compare("MyClient/1.0"));
    EXPECT_FALSE(req.headers["Accept"].compare("*/*"));
    EXPECT_TRUE(result == ParseCode::Success);

    /*
    std::cout << "Let's parse http request for 1,000,000 times!" << std::endl;
    alib::g3::Clock clk;
    for(int i = 0;i < 1'000'000;++i){
        req.parse(data);
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

TEST(URL, BasicParsing) {
    URL url;
    
    // 测试基本解码
    EXPECT_EQ(url.parse_raw_url("/hello%20world"), 0);
    EXPECT_EQ(url.full_path(), "/hello world");
    EXPECT_EQ(url.main_path(), "/hello world");
}

TEST(URL, QueryStringSeparation) {
    URL url;
    
    // 测试查询字符串分离
    EXPECT_EQ(url.parse_raw_url("/path%2Fto?name=value"), 0);
    EXPECT_EQ(url.full_path(), "/path/to?name=value");
    EXPECT_EQ(url.main_path(), "/path/to");  // 应该只到问号前
}

TEST(URL, EncodedQuestionMark) {
    URL url;
    
    // 测试编码的问号（应该在路径中）
    EXPECT_EQ(url.parse_raw_url("/path%3Fto/file?real=query"), 0);
    EXPECT_EQ(url.full_path(), "/path?to/file?real=query");
    EXPECT_EQ(url.main_path(), "/path?to/file");  // %3F解码的?在路径中
}

TEST(URL, PlusToSpace) {
    URL url;
    
    // 测试+号转空格
    EXPECT_EQ(url.parse_raw_url("/search+query?q=hello+world"), 0);
    EXPECT_EQ(url.full_path(), "/search query?q=hello world");
    EXPECT_EQ(url.main_path(), "/search query");
}

TEST(URL, InvalidEncoding) {
    URL url;
    
    // 测试非法编码
    url.parse_raw_url("/path%GG/file");
    EXPECT_FALSE(url.full_path().compare("/pathGG/file"));  // 非法十六进制
    url.parse_raw_url("/path%2/file");
    EXPECT_FALSE(url.full_path().compare("/path/file"));   // 不完整编码
}

TEST(URL, ControlCharacters) {
    URL url;
    
    // 测试控制字符过滤
    EXPECT_EQ(url.parse_raw_url("/safe%09tab"), 0);     // TAB允许
    EXPECT_EQ(url.parse_raw_url("/safe%0Anewline"), 0); // 换行允许  
    url.parse_raw_url("/danger%00null");
    EXPECT_FALSE(url.full_path().compare("/dangernull")); // NUL字符应该拒绝个屁，忽略就行
}

TEST(URL, NoQueryString) {
    URL url;
    
    // 测试没有查询字符串的情况
    EXPECT_EQ(url.parse_raw_url("/simple/path"), 0);
    EXPECT_EQ(url.full_path(), "/simple/path");
    EXPECT_EQ(url.main_path(), "/simple/path");
}

TEST(URL, ComplexExample) {
    URL url;
    
    // 复杂测试用例
    EXPECT_EQ(url.parse_raw_url("/api%2Fusers%2F123%3Faction%3Dview?page=1&size=20"), 0);
    EXPECT_EQ(url.full_path(), "/api/users/123?action=view?page=1&size=20");
    EXPECT_EQ(url.main_path(), "/api/users/123?action=view");
}

TEST(URL, EdgeCases) {
    URL url;
    
    // 边界情况测试
    EXPECT_EQ(url.parse_raw_url(""), 0);  // 空字符串
    EXPECT_EQ(url.parse_raw_url("?"), 0); // 只有问号
    EXPECT_EQ(url.parse_raw_url("%20"), 0); // 只有编码空格
}

TEST(URL, MultipleEncodings) {
    URL url;
    
    // 测试连续编码
    EXPECT_EQ(url.parse_raw_url("/%25%32%30test"), 0); // %25=% %32=2 %30=0 → %20=空格
    EXPECT_EQ(url.full_path(), "/%20test");
}

#endif