#include <http_parser.h>
#include <gtest/gtest.h>
#include <alib-g3/aclock.h>
// #include <format>

using namespace mnginx;

/// @todo WIP
void HTTPResponse::parse(std::string_view data){

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

void HTTPRequest::parse(std::string_view data){

}

/// @todo WIP
std::pmr::vector<char> HTTPRequest::generate(){
    std::pmr::vector<char> rdata;

    return rdata;
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