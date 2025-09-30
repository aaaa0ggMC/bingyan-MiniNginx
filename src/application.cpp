#include <application.h>

using namespace mnginx;

Application::Application():logger{LOG_SHOW_HEAD | LOG_SHOW_THID | LOG_SHOW_TIME | LOG_SHOW_TYPE},lg{"MiniNginx",logger}{

}

//// Setup Section ////
void Application::setup(){
    setup_general();
    setup_logger();
}

void Application::setup_general(){
    // Initialize cpp pmr resource pool,note that I'm not very familiar with it
    // I just know how to use
    resource = &pool;
    std::pmr::set_default_resource(resource);
}

void Application::setup_logger(){
    logger.appendLogOutputTarget("file",std::make_shared<lot::SplittedFiles>("./data/latest.log",4 * 1024 * 1024));
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
}

//// Main Section ////
void Application::run(){
    lg << "Hello World!" << endlog;

    HTTPRequest().parse(
                     "GET / HTTP/1.1\r\n"
                     "Host: example.com\r\n"
                     "User-Agent: MyClient/1.0\r\n"
                     "Accept: */*\r\n"
                     "\r\nHello World!");
}