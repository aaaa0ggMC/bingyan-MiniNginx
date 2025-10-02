/**
 * @file application.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief The Main Procedure
 * @version 0.1
 * @date 2025/09/30
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/09/30 
 */
#ifndef MN_APP
#define MN_APP 
#include <alib-g3/alogger.h>
#include <http_parser.h>
#include <memory_resource>
#include <epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <functional>

// Since my library is verbose,so here I globally use the namespace
using namespace alib::g3;

namespace mnginx{
    struct ClientInfo{
        std::pmr::vector<char> buffer;
        size_t find_last_pos;
        HTTPRequest pending_request;
        bool pending; 

        inline ClientInfo(){
            find_last_pos = 0;
            pending = false;
        }
    };

    enum class HandleResult{
        Continue,
        Close
    };

    enum class HandlerRule{
        FixedString,
        Match_Any
    };

    using HandlerFn = std::function<HandleResult(HTTPRequest &,const std::pmr::vector<std::pmr::string>&,HTTPResponse&)>;

    struct StateNode{
        HandlerRule rule;
        std::pmr::string data_str; // the user must make sure that data is usable during the processing
        bool empty;

        inline StateNode(){
            empty = true;
            data_str = "";
            rule = HandlerRule::FixedString;
        }

        inline StateNode& node(std::string_view data){
            data_str.clear();
            data_str.insert(data_str.end(),data.begin(),data.end());
            empty = false;
            next = std::make_unique<StateNode>();
            return *next;
        }

        /**
         * @brief create a node according to the rule
         * 
         * @param ru the rule,data is left empty
         * @return StateNode& 
         * @start-date 2025/10/02
         */
        inline StateNode& node(HandlerRule ru){
            data_str.clear();
            rule = ru; // when rule is Match_Any,data means the id of the context,leave empty to discard the param
            empty = false;
            next = std::make_unique<StateNode>();
            return *next;
        }

        std::unique_ptr<StateNode> next;
    };

    struct StateTree{
        struct Node{
            HandlerRule rule;
            std::pmr::string data_str;
            HandlerFn handler;

            std::pmr::vector<Node> nexts;
        };

        enum class AddResult : int32_t{
            Success,
            NoHandler,
            ConflictHandler,
            ContinueSearch,
            NextNode,
            PushToNexts,
            UnexpectedError,
            EmptyNodeName
        };

        enum class ParseResult : int32_t{
            OK,
            BadRoot,
            EmptyURL,
            Node404
        };

        Node root;

        AddResult add_new_handler(StateNode& tree,HandlerFn handler);
        // @todo wip,this function is not that essential
        // int delete_handler(StateNode& tree);
        void clear_tree();

        inline StateTree(){
            clear_tree();
        }

        ParseResult parseURL(std::string_view main_path,HandlerFn & fn,std::pmr::vector<std::pmr::string> & node_vals);

    };

    class Application{
    private:
        //// Log System
        Logger logger;
        LogFactory lg;
        
        //// Memory Management
        std::pmr::synchronized_pool_resource pool;
        std::pmr::memory_resource * resource;

        int server_fd;
        sockaddr_in address;
        EPoll epoll;

        std::pmr::unordered_map<int,ClientInfo> establishedClients; 
        StateTree handlers;
    public:
        int return_result;

        Application();
        ~Application();

        //// Setup Section ////
        void setup();
        void setup_general();
        void setup_server();
        void setup_logger();
        void setup_handlers();

        //// sub procedures ////
        void accept_connections();
        void handle_client(epoll_event & ev);
        bool cleanup_connection(int fd);
        void handle_pending_request(ClientInfo & client,int fd);
        /// note that code must lower than 1000
        size_t send_message_simp(int fd,HTTPResponse::StatusCode code,std::string_view stat_str);
        size_t send_message(int fd,HTTPResponse & resp,HTTPResponse::TransferMode = HTTPResponse::TransferMode::ContentLength);

        //// Main Section ////
        void run();
    };

}

#endif