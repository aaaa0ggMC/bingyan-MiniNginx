#include <http_parser.h>
#include <gtest/gtest.h>
// #include <format>

using namespace mnginx;

/// @todo WIP
void HTTPResponse::parse(std::string_view data){

}

std::pmr::vector<char> HTTPResponse::generate(){
    std::pmr::string pre_data;
    pre_data.resize(2048);
    //// Generate HTTP Version
    if(version.minor)pre_data = std::format("HTTP/{}.{} {} {}\r\n",
            version.major,version.minor,static_cast<int32_t>(status_code),status_str.c_str()
        );
    else pre_data = std::format("HTTP/{} {} {}\r\n",
            version.major,static_cast<int32_t>(status_code),status_str.c_str()
        );
    
    for(auto&[key,val] : headers){
        pre_data += key;
        // @todo @note we add a space after ':'
        pre_data += ": ";
        pre_data += val;
        pre_data += "\r\n";
    }
    pre_data += "\r\n";
    
    if(!!data){
        std::pmr::vector<char> rdata(pre_data.begin(),pre_data.end());
        HTTPData & d = *data;

        rdata.insert(rdata.end(),d.begin(),d.end());
        return rdata;
    }else{
        return std::pmr::vector<char>(pre_data.begin(),pre_data.end());
    }
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
    EXPECT_TRUE(response_str.find("HTTP/1.1 200 OK") != std::string::npos);
    EXPECT_TRUE(response_str.find("Content-Type: text/html") != std::string::npos);
    EXPECT_TRUE(response_str.find("Content-Length: ") != std::string::npos);
    if(resp.data){
        EXPECT_TRUE(response_str.find("Hello") != std::string::npos);
    }
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