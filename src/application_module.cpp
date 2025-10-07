#include <application.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stack>
#include <modules/reverse_proxy.h>
#include <modules/file_proxy.h>
#include <socket_util.h>

using namespace mnginx;

// read config and setup modules
void Application::setup_modules(){
    using namespace modules;

    auto server = config.config_nodes.get_node_recursive({"server"});
    if(!server){
        lge(LOG_WARN) << "No server section found in config" << endlog;
        return;
    }
    auto locations = server->get().get_child_nodes_view("location");
    for(auto & node : locations){
        auto path = node.get_value(0);
        if(!path){
            lge(LOG_WARN) << "No path provided for a node,skipped." << endlog;
            continue;
        }
        // build state tree
        StateNode root;
        int result = root.parse_from_str(*path);
        if(result){
            // @todo add more details
            lge(LOG_ERROR) << "Path \"" << *path << "\" is not valid!" << endlog;
            continue;
        }

        // expected for the first type set
        auto type = 
            node.get_node_recursive_value({"type"});
        if(!type){
            lge(LOG_WARN) << "Node with path \"" << *path  
                << "\" has no type signature." << endlog;
            continue;
        }
        if(*type == "proxy"){
            modules::ReverseClientConfig cfg;
            cfg.target.sin_family = AF_INET;
            // load server name
            auto str = node.get_node_recursive_value({"target","name"});
            if(str && !str->empty()){
                auto [ip_addr, error_msg] = mnginx::Util::get_s_addr_with_error(*str);
                if(ip_addr == INADDR_NONE){
                    lge(LOG_ERROR) << "Failed to resolve server address: \"" << *str << "\"" << endlog;
                    continue; //skip this node
                }else{
                    cfg.target.sin_addr.s_addr = ip_addr;
                }
            }
            // load port
            auto vport = node.get_node_recursive_value({"target","port"});
            if(vport && !vport->empty()){
                char * endptr;
                long port = strtol(vport->data(),&endptr,10);
                if(port <= 0 || port > 65535){
                    lge(LOG_ERROR) << "Port " << port << " is invalid!" << endlog;
                    continue;
                }else{
                    cfg.target.sin_port = htons(port);
                }
            }

            add_module<ModReverseProxy,policies<PolicyFull>>(root,cfg);

            lg(LOG_INFO) << "Proxy connections to " << mnginx::Util::ip_to_str(cfg.target.sin_addr.s_addr) 
                << ":" << ntohs(cfg.target.sin_port) << " with route \"" << 
                *path << "\"" << endlog; 
        }else if(*type == "file"){
            FileProxyConfig cfg;
            auto croot = node.get_node_recursive_value({"root"});
            if(croot){
                cfg.root = *croot;
            }
            auto index = node.get_node_recursive_value({"index"});
            if(index){
                cfg.defval = *index;
            }

            add_module<ModFileProxy,policies<PolicyInit>>(root,cfg);
        
            lg(LOG_INFO) << "Proxy file to " << cfg.root << " with index " 
                << cfg.defval << " and route \"" << *path << "\"" << endlog;
        }else{
            lge(LOG_WARN) << "Node with path \"" << *path  
                << "\" has bad type signature:" << *type << endlog;
            continue;
        }
    }
}