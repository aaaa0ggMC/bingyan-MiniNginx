#include <application.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stack>
#include <iostream>

using namespace mnginx;

Application::Application():
logger{LOG_SHOW_HEAD | LOG_SHOW_THID | LOG_SHOW_TIME | LOG_SHOW_TYPE},
lg{"MiniNginx",logger}{
    return_result = 0;
}

//// Setup Section ////
void Application::setup(){
    setup_general();
    setup_logger();
    setup_handlers();
    setup_servers();
}

void Application::setup_general(){
    // Initialize cpp pmr resource pool,note that I'm not very familiar with it
    // I just know how to use
    std::pmr::set_default_resource(&pool);
}

void Application::setup_servers(){
    server = std::make_unique<Server>(lg,lg,handlers);
    server->setup();
}

void Application::setup_logger(){
    logger.appendLogOutputTarget("file",std::make_shared<lot::SplittedFiles>("./data/latest.log",4 * 1024 * 1024));
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
}

void Application::setup_handlers(){
    StateNode file;
    file.node(HandlerRule::Match_Any);
    handlers.add_new_handler(file, [](HTTPRequest & rq, const std::pmr::vector<std::pmr::string>& vals, HTTPResponse& rp){
        rp.status_code = HTTPResponse::StatusCode::OK;
        rp.status_str = "OK";
        rp.headers["Content-Type"] = "text/html; charset=utf-8";
        rp.headers["Connection"] = "close";
        
        std::string first_val = vals.empty() ? "No Value" : std::string(vals[0].data(), vals[0].size());
        std::string html = "<html><body><h1>Hello World</h1><p>Value: " + first_val + "</p></body></html>";
        
        // 直接构造 HTTPData (pmr::vector<char>)
        rp.data.emplace(html.begin(), html.end());
        rp.headers[KEY_Content_Length] = std::to_string(rp.data->size());
        
        return HandleResult::Continue;
    });
    file = StateNode();
    file.node("file").node(HandlerRule::Match_Any);
    handlers.add_new_handler(file, [](HTTPRequest & rq, const std::pmr::vector<std::pmr::string>& vals, HTTPResponse& rp){
        rp.status_code = HTTPResponse::StatusCode::OK;
        rp.status_str = "OK";
        rp.headers["Content-Type"] = "text/plain; charset=utf-8";
        rp.headers["Connection"] = "close";
        
        // let's read
        std::string ou = "";
        std::string path ="/sorry/path/encrypted/";
        path += vals[1];
        alib::g3::Util::io_readAll(path,ou);
        
        std::cout << "PATH:" << path << std::endl;

        // 直接构造 HTTPData (pmr::vector<char>)
        rp.data.emplace(ou.begin(), ou.end());
        
        return HandleResult::Continue;
    });
}

//// Main Section ////
void Application::run(){
    server->run();
}