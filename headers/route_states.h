#ifndef MN_RSTATE
#define MN_RSTATE
#include <http_parser.h>
#include <functional>
#include <memory>

namespace mnginx{
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
} // namespace mnginx


#endif