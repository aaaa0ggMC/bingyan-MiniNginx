#include <application.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stack>
#include <iostream>
#include <modules/reverse_proxy.h>

using namespace mnginx;

uint64_t ClientInfo::max_client_id = 0;

Application::Application():
logger{LOG_SHOW_HEAD | LOG_SHOW_THID | LOG_SHOW_TIME | LOG_SHOW_TYPE},
lg{"MiniNginx",logger}{
    return_result = 0;
}

//// Setup Section ////
void Application::setup(){
    setup_general();
    setup_logger();
    setup_modules();
    setup_handlers();
    setup_servers();
}

void Application::setup_general(){
    // Initialize cpp pmr resource pool,note that I'm not very familiar with it
    // I just know how to use
    std::pmr::set_default_resource(&pool);
}

void Application::setup_logger(){
    logger.appendLogOutputTarget("file",std::make_shared<lot::SplittedFiles>("./data/latest.log",4 * 1024 * 1024));
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
}

void Application::setup_modules(){
    using namespace modules;
    StateNode root;
    root.node(HandlerRule::Match_Any);
    add_module<ModReverseProxy,PolicyFull>(root);
}

void Application::setup_handlers(){
    StateNode file;
    file.node("handler");
    handlers.add_new_handler(file, [](HandlerContext ctx){
        ctx.response.status_code = HTTPResponse::StatusCode::OK;
        ctx.response.status_str = "OK";
        ctx.response.headers["Content-Type"] = "text/html; charset=utf-8";
        ctx.response.headers["Connection"] = "close";
        
        std::string first_val = ctx.vals.empty() ? "No Value" : std::string(ctx.vals[0].data(), ctx.vals[0].size());
        std::string html = "<html><body><h1>Hello World</h1><p>Value: " + first_val + "</p></body></html>";
        
        // 直接构造 HTTPData (pmr::vector<char>)
        ctx.response.data.emplace(html.begin(), html.end());
        ctx.response.headers[KEY_Content_Length] = std::to_string(ctx.response.data->size());
        
        return HandleResult::Continue;
    });
}

void Application::setup_servers(){
    server = std::make_unique<Server>(lg,lg,handlers,mods);
    server->setup();
}

//// Main Section ////
void Application::run(){
    server->run();
}