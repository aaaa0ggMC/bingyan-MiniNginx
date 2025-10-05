#include <route_states.h>

using namespace mnginx;

StateTree::AddResult StateTree::add_new_handler(const StateNode& tree,HandlerFn handler){
    // step 1: check whether the tree has an handler
    if(!handler)return AddResult::NoHandler;
    Node * current = &root;
    const StateNode * pnode = &tree;
    
    auto proc_match_any = [&](Node & n){
        if(pnode->rule == HandlerRule::Match_Any){ // they are the same
            if(pnode->empty){
                if(!!n.handler)return AddResult::ConflictHandler;
                else{
                    n.handler = handler;
                    return AddResult::Success;
                }
            }else{
                // both move to the next node
                current = &n;
                pnode = &(*pnode->next);
                return AddResult::NextNode;
            }
        }
        return AddResult::ContinueSearch;
    };
    while(current && pnode){
        AddResult ret = AddResult::PushToNexts;
        for(Node & n : current->nexts){
            ret = AddResult::ContinueSearch;
            if(n.rule == HandlerRule::Match_Any){
                ret = proc_match_any(n);
            }else if(pnode->rule == HandlerRule::Match_Any){
                ret = AddResult::PushToNexts; // scan all and wait till break
            }else{
                if(!n.data_str.compare(pnode->data_str)){
                    // they are the same,move to next node
                    if(pnode->empty){ // there's no way to dive down!!
                        return AddResult::ConflictHandler;
                    }
                    current = &n;
                    pnode = &(*pnode->next);
                    ret = AddResult::NextNode;
                }
            }

            if(ret == AddResult::NextNode)break;
            else if(ret != AddResult::ContinueSearch && ret != AddResult::PushToNexts)return ret;
        }
        // search finished
        if(ret == AddResult::PushToNexts || ret == AddResult::ContinueSearch){ // we need to add the node
            Node node;
            node.data_str = (pnode->rule == HandlerRule::Match_Any)?"":pnode->data_str;
            node.rule = pnode->rule;
            node.id = pnode->id;
            if(pnode->empty){
                node.handler = handler;
                current->nexts.push_back(node);
                return AddResult::Success;
            }else{
                if(pnode->rule != HandlerRule::Match_Any && !pnode->data_str.compare(""))return AddResult::EmptyNodeName;
                current->nexts.push_back(node);
                current = &(current->nexts[current->nexts.size()-1]);
                pnode = &(*pnode->next);
            }
        }
    }
    return AddResult::UnexpectedError;
}

void StateTree::clear_tree(){
    root = Node();
    // the code below is not that essential too
    root.rule = HandlerRule::FixedString;
    root.data_str = "/";
}

StateTree::ParseResult StateTree::parseURL(std::string_view main_path,
    HandlerFn & fn,
    std::pmr::vector<std::pmr::string> & node_vals,
    std::pmr::unordered_map<std::string_view,int> & mapper){
    if(main_path.size() < 1)return ParseResult::EmptyURL;
    if(main_path[0] == '/')main_path = main_path.substr(1); // we leave space for path with no '/' prefix
    Node * current = &root;
    size_t pos = 0;

    node_vals.clear();
    mapper.clear();

    while(true){
        std::string_view name = "";
        Node * match_any = nullptr;
        bool find_ok = false;
        bool no_child = true;

        pos = main_path.find("/");
        if(pos != std::string_view::npos){
            name = std::string_view(main_path.begin(),main_path.begin() + pos);
        }else{
            name = std::string_view(main_path.begin(),main_path.end());
        }

        for(Node & n : current->nexts){
            if(no_child && !n.nexts.empty())no_child = false;
            if(n.rule == HandlerRule::Match_Any){
                match_any = &n;
                continue;
            }else if(!n.data_str.compare(name)){
                // next node
                if(n.nexts.size() == 0){
                    if(n.handler){
                        fn = n.handler;
                        if(!n.id.empty())mapper.emplace(n.id,node_vals.size());
                        return ParseResult::OK;
                    }else{
                        return ParseResult::Node404;
                    }
                }
                // node_vals will enlarge later
                if(!n.id.empty())mapper.emplace(n.id,node_vals.size());
                current = &n;
                find_ok = true;
            }
        }
        
        if(!find_ok){
            if(match_any){
                current = match_any;
                if(!match_any->id.empty())mapper.emplace(match_any->id,node_vals.size());
            }else if(no_child && current->nexts.size() == 1){ // that means we stepped into the end
                if(current->nexts[0].handler){
                    fn = current->nexts[0].handler;
                    if(node_vals.size() >= 1){
                        std::pmr::string & last = node_vals[node_vals.size()-1];
                        last.append("/");
                        last.append(main_path.begin(),main_path.end());
                    }
                    return ParseResult::OK; // now we added the "suffix"
                }else return ParseResult::Node404;
            }
            else return ParseResult::Node404;
        }

        node_vals.emplace_back(name.begin(),name.end());
        if(pos != std::string_view::npos)main_path = main_path.substr(pos + 1);
        else main_path = "";
    }

    return ParseResult::OK;
}

static std::pmr::string remove_unescapes(std::string_view data){
    bool escape = false;
    std::pmr::string ret;
    for(auto ch : data){
        if(ch == '\\'){
            if(escape){
                ret.push_back('\\');
            }else escape = true;
        }else if(escape){
            escape = false;
            ret.push_back(ch);
        }else ret.push_back(ch);
    }
    if(escape){
        ret.push_back('\\');
    }
    return ret;
}

int StateNode::parse_from_str(std::string_view str){
    if(str.empty())return -1;
    if(str[0] == '/')str = std::string_view(str.begin()+1,str.end());
    StateNode * current = this;
    std::string_view section = "";
    while(!str.empty()){
        size_t pos = 0;
        pos = str.find("/");
        if(pos != str.npos){
            section = std::string_view(str.begin(),str.begin() + pos);
            str = std::string_view(str.begin() + pos + 1,str.end());
        }else{
            section = str;
            str = "";
        }
        if(section == ""){
            return -2; // has something like this: /api// (// is forbidden)
        }
        current->empty = false;
        if(section.size() >= 2 && section[0] == '{' && section[section.size()-1] == '}'){
            current->rule = HandlerRule::Match_Any;
            current->data_str = "";
            size_t prev_index,aft_index;
            for(prev_index = 1;prev_index < section.size();++prev_index){
                if(!isspace(section[prev_index]))break;
            }
            for(aft_index = section.size()-2;aft_index > 0;--aft_index){
                if(!isspace(section[aft_index]))break;
            }
            current->id = remove_unescapes(std::string_view(section.begin() + prev_index,
                section.begin() + aft_index + 1));
            //std::cout << "read {ANY} :" << current->id << std::endl;
        }else{
            current->rule = HandlerRule::FixedString;
            current->data_str = remove_unescapes(section);
            current->id = "";
            //std::cout << "read {FIXED} :" << current->data_str << std::endl;
        }
        current->next = std::make_unique<StateNode>();
        current = current->next.get();
    }
    // make sure the last one is valid
    current->next = std::make_unique<StateNode>();
    return 0;
}