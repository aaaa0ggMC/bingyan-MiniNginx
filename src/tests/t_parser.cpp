#include <http_parser.h>
#include <gtest/gtest.h>
#include <alib-g3/autil.h>

using namespace mnginx;

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