#include <application.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stack>
#include <modules/reverse_proxy.h>
#include <socket_util.h>

using namespace mnginx;

uint64_t ClientInfo::max_client_id = 0;

Application::Application():
logger{LOG_SHOW_THID | LOG_SHOW_TIME | LOG_SHOW_TYPE},
lg{"MiniNginx",logger},
lgerr{LOG_SHOW_THID | LOG_SHOW_TIME | LOG_SHOW_TYPE},
lge{"MiniNgnix",lgerr}{
    return_result = 0;
}

Application::~Application(){
    logger.setLogOutputTargetStatus("console",false); 
    lgerr.setLogOutputTargetStatus("console",false); 
    lg(LOG_INFO) << "=== MiniNginx Access Log Ended ===" << endlog;
    lge(LOG_INFO) << "=== MiniNginx Error Log Ended ===" << endlog;
    logger.flush();
    lgerr.flush();


    logger.setLogOutputTargetStatus("file",false); 
    logger.setLogOutputTargetStatus("console",true); 
    lg(LOG_INFO) << "Application terminated" << endlog;
}

//// Setup Section ////
void Application::setup(){
    setup_general();
    setup_config();
    setup_logger();
    setup_modules();
    setup_handlers();
    setup_servers();
}

void Application::setup_general(){
    // Initialize cpp pmr resource pool,note that I'm not very familiar with it
    // I just know how to use
    std::pmr::set_default_resource(&pool);
    // create logger console target
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
    lgerr.appendLogOutputTarget("console",std::make_shared<lot::Console>());
}

// app setup config see application_config.cpp

void Application::setup_logger(){
    logger.appendLogOutputTarget("file",std::make_shared<lot::SplittedFiles>("./data/latest-acc.log",4 * 1024 * 1024));
    lgerr.appendLogOutputTarget("file",std::make_shared<lot::SplittedFiles>("./data/latest-err.log",4 * 1024 * 1024));
    logger.setLogOutputTargetStatus("console",false); 
    lgerr.setLogOutputTargetStatus("console",false); 
    lg(LOG_INFO) << "=== MiniNginx Access Log Started ===" << endlog;
    lge(LOG_INFO) << "=== MiniNginx Error Log Started ===" << endlog;
    logger.setLogOutputTargetStatus("console",true); 
    lgerr.setLogOutputTargetStatus("console",true);
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
    server = std::make_unique<Server>(lg,lge,handlers,mods);
    server->config = cfg_server;
    server->setup();
}

//// Main Section ////
void Application::run(){
    server->run();
}