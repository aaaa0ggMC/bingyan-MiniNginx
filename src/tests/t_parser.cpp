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

#include <gtest/gtest.h>
#include <http_parser.h>

using namespace mnginx;

TEST(HTTPRequestGenerateTest, BasicGETRequest) {
    HTTPRequest req;
    req.method = HTTPRequest::HTTPMethod::GET;
    req.url.parse_raw_url("/api/users/123");
    req.version = {1, 1};
    req.headers["Host"] = "example.com";
    req.headers["User-Agent"] = "MiniNginx-Test";
    
    std::pmr::vector<char> buffer;
    req.generate_to(buffer);
    
    std::string result(buffer.begin(), buffer.end());
    std::cout << "=== Basic GET Request ===" << std::endl;
    std::cout << result << std::endl;
    
    EXPECT_NE(result.find("GET /api/users/123 HTTP/1.1"), std::string::npos);
    EXPECT_NE(result.find("Host: example.com"), std::string::npos);
    EXPECT_NE(result.find("User-Agent: MiniNginx-Test"), std::string::npos);
}

TEST(HTTPRequestGenerateTest, POSTRequestWithBody) {
    HTTPRequest req;
    req.method = HTTPRequest::HTTPMethod::POST;
    req.url.parse_raw_url("/api/users");
    req.version = {1, 1};
    req.headers["Host"] = "api.example.com";
    req.headers["Content-Type"] = "application/json";
    req.headers["Content-Length"] = "26";
    
    std::string json_body = R"({"name":"john","age":30})";
    req.data = std::pmr::vector<char>(json_body.begin(), json_body.end());
    
    std::pmr::vector<char> buffer;
    req.generate_to(buffer);
    
    std::string result(buffer.begin(), buffer.end());
    std::cout << "=== POST Request with Body ===" << std::endl;
    std::cout << result << std::endl;
    
    EXPECT_NE(result.find("POST /api/users HTTP/1.1"), std::string::npos);
    EXPECT_NE(result.find("Content-Type: application/json"), std::string::npos);
    EXPECT_NE(result.find("Content-Length: 26"), std::string::npos);
    EXPECT_NE(result.find(R"({"name":"john","age":30})"), std::string::npos);
}

TEST(HTTPRequestGenerateTest, HTTP10Request) {
    HTTPRequest req;
    req.method = HTTPRequest::HTTPMethod::GET;
    req.url.parse_raw_url("/");
    req.version = {1, 0};
    req.headers["Host"] = "simple.example.com";
    
    std::pmr::vector<char> buffer;
    req.generate_to(buffer);
    
    std::string result(buffer.begin(), buffer.end());
    std::cout << "=== HTTP/1.0 Request ===" << std::endl;
    std::cout << result << std::endl;
    
    EXPECT_NE(result.find("GET / HTTP/1"), std::string::npos);
    EXPECT_EQ(result.find("GET / HTTP/1.0"), std::string::npos);
    EXPECT_NE(result.find("Host: simple.example.com"), std::string::npos);
}

TEST(HTTPRequestGenerateTest, ComplexURLWithQuery) {
    HTTPRequest req;
    req.method = HTTPRequest::HTTPMethod::GET;
    req.url.parse_raw_url("/search?q=hello+world&page=1&limit=10");
    req.version = {1, 1};
    req.headers["Host"] = "search.example.com";
    req.headers["Accept"] = "application/json";
    
    std::pmr::vector<char> buffer;
    req.generate_to(buffer);
    
    std::string result(buffer.begin(), buffer.end());
    std::cout << "=== Request with Query String ===" << std::endl;
    std::cout << result << std::endl;
    
    EXPECT_NE(result.find("GET /search?q=hello+world&page=1&limit=10 HTTP/1.1"), std::string::npos);
    EXPECT_NE(result.find("Accept: application/json"), std::string::npos);
}

TEST(HTTPRequestGenerateTest, MultipleHeaders) {
    HTTPRequest req;
    req.method = HTTPRequest::HTTPMethod::GET;
    req.url.parse_raw_url("/");
    req.version = {1, 1};
    req.headers["Host"] = "multi.example.com";
    req.headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
    req.headers["Accept-Language"] = "en-US,en;q=0.5";
    req.headers["Accept-Encoding"] = "gzip, deflate, br";
    req.headers["Connection"] = "keep-alive";
    
    std::pmr::vector<char> buffer;
    req.generate_to(buffer);
    
    std::string result(buffer.begin(), buffer.end());
    std::cout << "=== Request with Multiple Headers ===" << std::endl;
    std::cout << result << std::endl;
    
    EXPECT_NE(result.find("Host: multi.example.com"), std::string::npos);
    EXPECT_NE(result.find("Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"), std::string::npos);
    EXPECT_NE(result.find("Accept-Language: en-US,en;q=0.5"), std::string::npos);
    EXPECT_NE(result.find("Accept-Encoding: gzip, deflate, br"), std::string::npos);
    EXPECT_NE(result.find("Connection: keep-alive"), std::string::npos);
}

TEST(HTTPRequestGenerateTest, EmptyBodyRequest) {
    HTTPRequest req;
    req.method = HTTPRequest::HTTPMethod::GET;
    req.url.parse_raw_url("/health");
    req.version = {1, 1};
    req.headers["Host"] = "health.example.com";
    // 明确不设置 data
    
    std::pmr::vector<char> buffer;
    req.generate_to(buffer);
    
    std::string result(buffer.begin(), buffer.end());
    std::cout << "=== Request with Empty Body ===" << std::endl;
    std::cout << result << std::endl;
    
    // 检查是否以 \r\n\r\n 结尾（没有消息体）
    EXPECT_EQ(result.substr(result.length() - 4), "\r\n\r\n");
}

TEST(HTTPRequestGenerateTest, PerformanceComparison) {
    HTTPRequest req;
    req.method = HTTPRequest::HTTPMethod::GET;
    req.url.parse_raw_url("/api/test");
    req.version = {1, 1};
    req.headers["Host"] = "perf.example.com";
    req.headers["User-Agent"] = "Performance-Test";
    
    // 测试 generate() 方法（有 RVO）
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        auto data = req.generate();
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // 测试 generate_to() 方法（零分配）
    std::pmr::vector<char> buffer;
    buffer.reserve(2048);
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        req.generate_to(buffer);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    
    std::cout << "=== Performance Comparison ===" << std::endl;
    std::cout << "generate() with RVO: " << duration1.count() << "μs for 10,000 calls" << std::endl;
    std::cout << "generate_to() zero-allocation: " << duration2.count() << "μs for 10,000 calls" << std::endl;
    std::cout << "Performance improvement: " << (double)duration1.count() / duration2.count() << "x" << std::endl;
}