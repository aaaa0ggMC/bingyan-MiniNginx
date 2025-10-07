/**
 * @file route_states.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief A state machines that reads url and output handlers to process the url
 * support wildcard match and precise match
 * @version 0.1
 * @date 2025/10/02
 * @copyright Copyright(c)2025 aaaa0ggmc
 * @start-date 2025/10/02 
 */
#ifndef MN_RSTATE
#define MN_RSTATE
#include <http_parser.h>
#include <client.h>
#include <functional>
#include <memory>

namespace mnginx{
    // @todo this feature hasnt been implemented yet
    /// result returned by Handler
    enum class HandleResult{
        Continue, ///< keep the connection with current client
        Close, ///< close the connection with current client
        AlreadySend ///< you have already sent something to the client,the server wont use reposne to send again
    };

    /**
     * @brief some rules used to specify the relationship between router and handler
     * @par example: 
     * fixed(/)fixed(api) -> handlera
     * fixed(/)match_any() -> handlerb
     * 
     * then: /api -> a,/api/fdfdsf -> a(if there no extra nodes,
     *      the rest of the path will be added)
     * /index -> b
     */
    enum class HandlerRule{
        FixedString, ///< a fixed string,has higher priority than match any
        Match_Any ///< can match any string of the entry
    };

    /// args need to pass to handler
    struct HandlerContext{
        int fd; ///< file descriptor
        HTTPRequest & request; ///< incoming request,= client_info.pending_request
        std::pmr::vector<std::pmr::string>& vals; ///< path vals
        HTTPResponse& response; ///< output,it has been reset before
        ClientInfo & client; ///< something about the client
        /// map id to the index in val,string_view refers to data stored in StateTree,so it's safe usually
        std::pmr::unordered_map<std::string_view,int>& mapper; 

        inline std::optional<std::string_view> get_param(std::string_view id){
            auto it = mapper.find(id);
            if(it == mapper.end() || it->second >= vals.size())return std::nullopt;
            return vals[it->second];
        }

        inline HandlerContext(
            int fd,
            HTTPRequest& rq,
            std::pmr::vector<std::pmr::string>& nv,
            HTTPResponse& rp,
            ClientInfo & cl,
            std::pmr::unordered_map<std::string_view,int>& _mapper
        ):fd{fd},request{rq},vals{nv},response{rp},client{cl},mapper{_mapper}{}
    };

    /// function type to handle routes
    using HandlerFn = std::function<HandleResult(HandlerContext)>;

    /**
     * @brief A user-oriented class for the user to build an route chain
     * @par Examples:
     * root.node("api").node("v1").node(HandlerRule::Match_Any) ==> /api/v1/{anyval}
     * @start-date 2025/10/02
     */
    struct StateNode{
        /// match rule of the current node
        HandlerRule rule;
        /// if rule=FixedString,this is the value of the string
        /// if rule=Match_Any,It must be ""
        std::pmr::string data_str;
        /// to check whether the chain reached to the end
        bool empty;
        /// id of the node
        std::pmr::string id;

        /// constructor,initializes StateNode with default value
        /// @note if a node has empty data_str but rule is FixedString,add_new_handler will return EmptyNodeName 
        inline StateNode(){
            empty = true;
            data_str = "";
            id = "";
            rule = HandlerRule::FixedString;
        }

        /**
         * @brief with fixedstring,create a rule for this node and create next node
         * @param data FixedString
         * @return StateNode& 
         * @start-date 2025/10/03
         */
        inline StateNode& node(std::string_view data,std::string_view i_id = ""){
            data_str.clear();
            data_str.insert(data_str.end(),data.begin(),data.end());
            id.clear();
            id.insert(id.end(),i_id.begin(),i_id.end());
            empty = false;
            next = std::make_unique<StateNode>();
            return *next;
        }

        /**
         * @brief with Match_Any,create a node according to the rule
         * 
         * @param ru the rule,data is left empty
         * @return StateNode& 
         * @start-date 2025/10/02
         */
        inline StateNode& node(HandlerRule ru,std::string_view i_id = ""){
            data_str.clear();
            id.clear();
            id.insert(id.end(),i_id.begin(),i_id.end());
            rule = ru; // when rule is Match_Any,data means the id of the context,leave empty to discard the param
            empty = false;
            next = std::make_unique<StateNode>();
            return *next;
        }

        /// parse a node list from string
        /// eg. /api/v1/{id}/{id2}, use \{ \} to enter real '{' and '}'
        int parse_from_str(std::string_view str);

        /// next node
        std::unique_ptr<StateNode> next;
    };

    /**
     * @brief The state machine held by master(mnginx::Application)
     * @start-date 2025/10/03
     */
    struct StateTree{
        /// result of add nodes
        enum class AddResult : int32_t{
            Success, ///< OK
            NoHandler, ///< param handler is nullptr
            ConflictHandler, ///< the route has a handler already
            UnexpectedError, ///< you will never meet this,im sure
            EmptyNodeName, ///< node name is empty and the rule is FixedString
            //// Used in sub-procedures,will never be returned by add_new_handler
            ContinueSearch, ///< continue to search for matched node
            NextNode, ///< move to next depth of nodes
            PushToNexts, ///< add a new node to nexts(the child node vector)
        };

        /// result of parseURL
        enum class ParseResult : int32_t{
            OK, ///< OK
            EmptyURL, ///< requested url is empty
            Node404 ///< cant find a valid handler for the current route
        };


        /// internal Node struct to create a state tree
        struct Node{
            /// match rule of the current node
            HandlerRule rule;
            /// if rule=FixedString,this is the value of the string
            /// if rule=Match_Any,It must be ""
            std::pmr::string data_str;
            /// handler of current node
            HandlerFn handler;
            /// id
            std::pmr::string id;

            /// child nodes
            std::pmr::vector<Node> nexts;
        };

        /// Root node
        Node root;
        
        /**
         * @brief add new handler to the state machine
         * 
         * @param tree user-defined chain
         * @param handler handler of the chain
         * @return AddResult 
         * @start-date 2025/10/03
         */
        AddResult add_new_handler(const StateNode& tree,HandlerFn handler);
        
        // @todo wip,this function is not that essential
        // int delete_handler(StateNode& tree);

        /// clear the tree with default root
        void clear_tree();        

        /**
         * @brief parse an url and get related Handler
         * @param main_path from URL::main_path()
         * @param fn where the found handler to be stored in
         * @param node_vals the node vals,eg. RULE(/api/hello/world) IN(/api/hello/world/nihao) OUT([api,hello,world/nihao])
         * @return ParseResult 
         * @start-date 2025/10/03
         */
        ParseResult parseURL(std::string_view main_path,
            HandlerFn & fn,std::pmr::vector<std::pmr::string> & node_vals,
            std::pmr::unordered_map<std::string_view,int> & mapper);

        /// construct the tree with root whose value is "/" and rule is FixedString
        /// (PS.if you changed it,nothing will changed,the data except nexts of root wont be used)
        inline StateTree(){
            clear_tree();
        }
    };
} // namespace mnginx
#endif