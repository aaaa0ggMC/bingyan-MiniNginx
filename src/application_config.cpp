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

void Application::setup_config(){
    std::string config_data = "";
    alib::g3::Util::io_readAll("./data/config",config_data);
    switch(config.load_from_buffer(config_data)){
    case Config::LoadResult::EOFTooEarly:
        lge(LOG_ERROR) << "Config syntax incomplete" << endlog; 
        break;
    case Config::LoadResult::WrongGrammar:
        lge(LOG_ERROR) << "Config syntax error" << endlog;
        break;
    case Config::LoadResult::OK:
        break;
    default:
        lge(LOG_ERROR) << "Unknown config error" << endlog;
        break;
    }

    std::string states = "";
    //// Init server configs ////
    cfg_server.address.sin_family = AF_INET;
    // [A] server_name
    auto str = config.config_nodes.get_node_recursive_value({"server","server_name"});
    if(str && !str->empty()){
        auto [ip_addr, error_msg] = mnginx::Util::get_s_addr_with_error(*str);
        if(ip_addr == INADDR_NONE){
            lge(LOG_ERROR) << "Failed to resolve server address: \"" << *str << "\"" << endlog;
        }else{
            lg(LOG_DEBUG) << "Loaded server name \"" << *str <<
                "\" --> \"" << ip_addr << "\"" << endlog;  
            states += "A";
            cfg_server.address.sin_addr.s_addr = ip_addr;
        }
    }
    // [B] listen port
    str = config.config_nodes.get_node_recursive_value({"server","listen"});
    if(str && !str->empty()){
        char * endptr;
        long port = strtol(str->data(),&endptr,10);
        if(port <= 0 || port > 65535){
            lge(LOG_ERROR) << "Port " << port << " is invalid,use "
             << ntohs(cfg_server.address.sin_port) << " as fallback value" << endlog;
        }else if(port < 1024 && port != 80 && port != 443 && port != 8080) {
            lg(LOG_WARN) << "Using privileged port (may require root): " << port << endlog;
            cfg_server.address.sin_port = htons(port);
            states += "B";
            lg(LOG_DEBUG) << "Loaded port " << port << endlog;
        }else{
            cfg_server.address.sin_port = htons(port);
            lg(LOG_DEBUG) << "Loaded port " << port << endlog;
            states += "B";
        }
    }
    // [C] backlog count
    str = config.config_nodes.get_node_recursive_value({"server","backlog"});
    if(str && !str->empty()){
        char * endptr;
        long backlog_count = strtol(str->data(),&endptr,10);
        long safe_count = mnginx::Util::safe_back_log(backlog_count);

        if(safe_count != backlog_count){
            lge(LOG_WARN) << "Backlog too large, using system maximum: " << safe_count << endlog;
        }
        cfg_server.backlog_count = safe_count;
        lg(LOG_DEBUG) << "Loaded backlog count " << safe_count << endlog;
        states += "C";
    }
    // [D] epoll ev size
    str = config.config_nodes.get_node_recursive_value({"server","epoll","event_list_size"});
    if(str && !str->empty()){
        char * endptr;
        long ev_len = strtol(str->data(),&endptr,10);
        if(ev_len <= 0){
            lge(LOG_WARN) << "Epoll event list size is bigger than 0!Current:" << ev_len << endlog;
        }else{
            cfg_server.epoll_event_list_size = ev_len;
            lg(LOG_DEBUG) << "Loaded event list length " << ev_len << endlog;
            states += "D";
        }
    }
    // [E] epoll wait interval 
    str = config.config_nodes.get_node_recursive_value({"server","epoll","wait_interval"});
    if(str && !str->empty()){
        char * endptr;
        long wt = strtol(str->data(),&endptr,10);
        // units
        str = config.config_nodes.get_node_recursive_value({"server","epoll","wait_interval"},0,1);
        if(str){
            if(!str->compare("ms"))wt = wt;
            else if(!str->compare("us"))wt /= 1000;
            else if(!str->compare("s"))wt *= 1000;
            else if(!str->compare("min"))wt *= 1000 * 60;
            else if(!str->compare("d"))wt *= 1000 * 60 * 24;
            else{
                lge(LOG_ERROR) << "Unrecognized format " << *str << endlog;
            }
        }
        if(wt > 16'000){
            lge(LOG_WARN) << "Epoll wait interval is over 16 seconds,may be too long!" << endlog;
        }
        cfg_server.epoll_event_list_size = wt;
        lg(LOG_DEBUG) << "Loaded epoll wait interval " << wt << "ms" << endlog;
        states += "E";
    }

    // [F] module at least wait time    
    str = config.config_nodes.get_node_recursive_value({"server","epoll","module_least_wait"});
    if(str && !str->empty()){
        char * endptr;
        long wt = strtol(str->data(),&endptr,10);
        if(wt >= 0){
            // units
            str = config.config_nodes.get_node_recursive_value({"server","epoll","module_least_wait"},0,1);
            if(str){
                if(!str->compare("ms"))wt = wt;
                else if(!str->compare("us"))wt /= 1000;
                else if(!str->compare("s"))wt *= 1000;
                else if(!str->compare("min"))wt *= 1000 * 60;
                else if(!str->compare("d"))wt *= 1000 * 60 * 24;
                else{
                    lge(LOG_ERROR) << "Unrecognized format " << *str << endlog;
                }
            }
            if(wt > 1000){
                lge(LOG_WARN) << "Module least wait time is over 1 second,may be too long!" << endlog;
            }
            cfg_server.module_timer_at_least_wait_ms = wt;
            states += "F";
            lg(LOG_DEBUG) << "Loaded module least wait time: " << wt << "ms" << endlog;
        }else{
            lge(LOG_WARN) << "Module least wait time is bigger than 0!Current:" << wt << endlog;
        }
    }

    // [G] HTTP PACKAGE MAX SIZE
    str = config.config_nodes.get_node_recursive_value({"server","http_package_max_size"});
    if(str && !str->empty()){
        char * endptr;
        long ev_len = strtol(str->data(),&endptr,10);
        if(ev_len <= 0){
            lge(LOG_WARN) << "HTTP max package size is bigger than 0!Current:" << ev_len << endlog;
        }else{
            cfg_server.http_package_max_size = ev_len;
            lg(LOG_DEBUG) << "Loaded HTTP max package size: " << ev_len << endlog;
            states += "G";
        }
    }

    // [H] Worker Count
    str = config.config_nodes.get_node_recursive_value({"server","workers"});
    if(str && !str->empty()){
        char * endptr;
        long ev_len = strtol(str->data(),&endptr,10);
        if(ev_len <= 0){
            lge(LOG_WARN) << "Worker count is bigger than 0!Current:" << ev_len << endlog;
        }else{
            cfg_server.workers = ev_len;
            lg(LOG_DEBUG) << "Loaded worker count: " << ev_len << endlog;
            states += "H";
        }
    }

    lg(LOG_INFO) << "Config updated:" << states << endlog;
}