#include <http_parser.h>
#include <alib-g3/aclock.h>

using namespace mnginx;

//// URL ////
int URL::parse_raw_url(std::string_view raw){
    static auto acceptable = [](long ch){
        if(ch == 0x09 || ch == 0x0A || ch == 0x0D)return true;
        else if(ch <= 0x1F || ch == 0x7F || ch > 0xFF)return false;
        else return true;
    };
    // @todo search and add errors
    int64_t pos = (int64_t)raw.find("?");
    if(pos == std::string_view::npos)pos = -1;
    int64_t original = pos;

    // assign raw path data
    if(raw.empty()){
        raw_path_ = "/";
    }else{
        if(raw[0] != '/')raw_path_ = "/";
        else raw_path_ = "";
        raw_path_ += raw;
    }
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

//// HTTPRequest ////
// 请求的网页可能带非法字符比如空格，因此有%20的转换！(I've done this)
/* perf: 2028.07ms for 1'000'000 calls   ==> 2.02807us / call*/
ParseCode HTTPRequest::parse(std::string_view str){
    auto pos = str.find(' ');
    auto oldpos = 0;
    //// Extract Method
    if(pos == std::string_view::npos)return ParseCode::InvalidFormat;
    method = getMethod(str.substr(0,pos));
    if(method == HTTPMethod::InvalidMethod){
        return ParseCode::InvalidMethod;
    }

    //// Extract URL
    oldpos = pos + 1;
    pos = str.substr(oldpos).find(' ') + oldpos;
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
        pos = str.substr(oldpos).find("\r\n");
        if(pos == std::string_view::npos)return ParseCode::InvalidFormat; // So,there's no ending
        else if(pos == 0) {
            pos = oldpos; // keep the right value
            break; // We reached to the end line
        }

        // Normal Line
        std::string_view full_line = str.substr(oldpos,pos);
        pos += oldpos;
        if(is_space(full_line[0]))return ParseCode::SpaceInTheFrontOfHeader; // "  Data: Host" is forbidden
        auto colon_pos = full_line.find(":");
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
            if(i > 32){
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

std::pmr::vector<char> HTTPRequest::generate(){
    std::pmr::vector<char> rdata;
    rdata.reserve(2048);
    generate_to(rdata);
    return rdata;
}

void HTTPRequest::generate_to(std::pmr::vector<char> & rdata){
    using data_t = std::pmr::vector<char>;
    rdata.clear();
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
            // well you win
        }// status code < 1000
    };

    //// Request Method ////
    auto method_str = getMethodString(method);
    rdata.insert(rdata.end(),method_str.begin(),method_str.end());
    rdata.push_back(' ');
    //// URL ////
    rdata.insert(rdata.end(),url.raw_path().begin(),url.raw_path().end());
    //// HTTP Version ////
    rdata.insert(rdata.end(),{' ','H','T','T','P','/'});
    append_num(rdata,version.major);
    if(version.minor){
        rdata.push_back('.');
        append_num(rdata,version.minor);
    }
    rdata.insert(rdata.end(),{'\r','\n'});
    
    //// headers ////
    for(auto& [k,v] : headers){
        rdata.insert(rdata.end(),k.begin(),k.end());
        rdata.insert(rdata.end(),{':',' '});
        rdata.insert(rdata.end(),v.begin(),v.end());
        rdata.insert(rdata.end(),{'\r','\n'});
    }
    rdata.insert(rdata.end(),{'\r','\n'});

    //// message body ////
    if(data && (data->size() != 0)){
        rdata.insert(rdata.end(),data->begin(),data->end());
    }
}

HTTPRequest::HTTPMethod HTTPRequest::getMethod(std::string_view str){
    using M = HTTPRequest::HTTPMethod;
    int str_size = str.size();
    //discard some useless data
    if(str_size <= 2)return M::InvalidMethod;
    
    if(!strncmp(str.data(),"GET",3))return M::GET;
    else if(!strncmp(str.data(),"POST",4))return M::POST;
    else if(!strncmp(str.data(),"HEAD",4))return M::HEAD;
    else if(!strncmp(str.data(),"PUT",3))return M::PUT;
    else if(!strncmp(str.data(),"CONNECT",5))return M::CONNECT;
    else if(!strncmp(str.data(),"PATCH",5))return M::PATCH;
    else if(!strncmp(str.data(),"DELETE",6))return M::DELETE;
    else if(!strncmp(str.data(),"OPTIONS",7))return M::OPTIONS;
    else if(!strncmp(str.data(),"TRACE",6))return M::TRACE;
    else return M::InvalidMethod;
}

std::string_view HTTPRequest::getMethodString(HTTPRequest::HTTPMethod method){
    using M = HTTPRequest::HTTPMethod;

#define CREATE_NEW(X) case M::X : return ""#X;

    switch(method){
    CREATE_NEW(PUT);
    CREATE_NEW(POST);
    CREATE_NEW(CONNECT);
    CREATE_NEW(TRACE);
    CREATE_NEW(OPTIONS);
    CREATE_NEW(PATCH);
    CREATE_NEW(HEAD);
    CREATE_NEW(GET);
    CREATE_NEW(DELETE);
    default:
        return "INVALID";
    }
#undef CREATE_NEW
}

//// HTTP Response ////
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
    generate_to(rdata);
    return rdata;
}

void HTTPResponse::generate_to(std::pmr::vector<char>& rdata) const{
    using data_t = std::pmr::vector<char>;
    
    rdata.clear();
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
            // well you win
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
    
    if(data && data->size() != 0){
        rdata.insert(rdata.end(),data->begin(),data->end());
    }
}

void HTTPResponse::checkout_data(TransferMode mode){
    switch(mode){
    case TransferMode::ContentLength:
        // @todo use performant solutions to cast int to string
        headers[KEY_Content_Length] = std::to_string((data)?data->size():0);
        break;
    case TransferMode::Chunked:
        // @todo not supported now
        // @todo LOG
        break;
    default:
        break;
    }
}

void HTTPResponse::reset(){
    version.major = 1;
    version.minor = 1;
    status_str = "OK";
    status_code = StatusCode::OK;
    headers.clear();
    if(!!data)data->clear();
}