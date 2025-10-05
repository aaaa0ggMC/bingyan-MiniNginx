#include <config.h>
#include <fstream>
#include <stack>

using namespace mnginx;

Config::LoadResult Config::load_from_buffer(std::string_view data){
    if(data.empty())return LoadResult::EOFTooEarly;
    std::vector<std::string> tokens;
    std::string buffer = "";

    bool in_str = false;
    bool escape = false;

    size_t i = 0;

    char ch;

    while(i < data.size()){
        ch = data[i];
        ++i;
        if(in_str){
            if(escape){
                escape = false;
                switch(ch){
                case '\\':
                case '\"':
                case '\'':
                    buffer.push_back(ch);
                    break;
                default:
                    buffer.push_back('\\');
                    buffer.push_back(ch);
                }
                continue;
            }else{
                if(ch == '\\'){
                    escape = true;
                    continue;
                }else if(ch == '\"'){
                    in_str = false;
                    // and we finished a token
                    buffer = "\"" + buffer + "\""; // to prevent "{" "}"
                    tokens.push_back(buffer);
                    buffer.clear();
                    continue;
                }
                buffer.push_back(ch);
            }
        }else{
            if(ch == '\"'){
                in_str = true;
                continue;
            }
            if(isspace(ch)){
                if(!buffer.empty()){
                    // we finished a token
                    tokens.push_back(buffer);
                    buffer.clear();
                }
                continue;
            }
            if(ch == '{' || ch == '}' || ch == ';'){
                // finshed the prev token and add this token along
                if(!buffer.empty()){
                    tokens.push_back(buffer);
                    buffer.clear();
                }
                tokens.emplace_back();
                tokens[tokens.size()-1].push_back(ch);
                continue;
            }
            buffer.push_back(ch);
        }
    }

    return analyse_words(tokens);
}

Config::LoadResult Config::analyse_words(std::vector<std::string> & tokens){
    std::stack<Node*> tree;
    /// clear data
    config_nodes = Node();
    config_nodes.children.push_back(Node());
    Node * current = &config_nodes.children[0];

    tree.push(&config_nodes);
    tree.push(current);
    

    for(size_t i = 0;i < tokens.size();){
        auto & tk = tokens[i];
        ++i;
        if(!tk.compare("{")){
            if(!current->name.compare("")){
                // at least a name is required
                return LoadResult::WrongGrammar; 
            }
            // to the next depth of node
            current->children.push_back(Node());
            tree.push(&(*(current->children.end()-1)));
            current = tree.top(); // move to child
        }else if(!tk.compare("}")){
            if(tree.empty()){
                // @todo add more infos
                return LoadResult::WrongGrammar;
            }
            // to the prev depth of node
            tree.pop();
            bool dec = false;
            if(!current->name.compare(""))dec = true;
            if(tree.empty())return LoadResult::WrongGrammar;
            current = tree.top();
            if(dec && current->children.size() > 0){
                current->children.erase(current->children.end()-1);
            }
            tree.pop();
            if(tree.empty())return LoadResult::WrongGrammar;
            current = tree.top();
            // move to next 
            current->children.push_back(Node());
            tree.push(&(*(current->children.end()-1)));
            current = tree.top();
        }else if(!tk.compare(";")){
            if(!current->name.empty()){
                // add new node
                tree.pop();
                if(tree.empty())return LoadResult::WrongGrammar;
                current = tree.top();
                current->children.push_back(Node());
                tree.push(&(*(current->children.end()-1)));
                current = tree.top();
            }
            continue;
        }else{
            if(current->name.empty())current->name = tk;
            else current->values.emplace_back(tk);
        }
    }

    size_t sz = config_nodes.children.size();
    if(sz && !config_nodes.children[sz-1].name.compare("")){
        config_nodes.children.erase(config_nodes.children.end() -1);
    }
    return LoadResult::OK;
}

std::optional<std::reference_wrapper<Config::Node>> Config::Node::get_node_recursive(const std::vector<std::string_view> & location,size_t expected_index){
    Node * current = this;
    size_t i = 0,cmp_index = 0;
    size_t founded = 0;
    size_t last_find = std::string::npos; 

    auto reset = [&]{
        last_find = std::string::npos;
        founded = 0;
        i = 0;
    };

    do{
        while(cmp_index < location.size() && i < current->children.size()){
            if(current->children[i].name == location[cmp_index]){
                // find the last item
                if(cmp_index == location.size()-1 && founded < expected_index){
                    ++founded;
                    last_find = i;
                    i++;
                    continue;
                }else{
                    //OK,move to next node
                    current = &(current->children[i]);
                    reset();
                    ++cmp_index;
                    continue; // to prevent i from adding
                }
            }else i++; 
        }
        if(founded <= expected_index && last_find != std::string::npos){
            current = &(current->children[last_find]);
            reset();
            ++cmp_index;
            continue;
        }
    }while(0);
    // find nothing
    if(cmp_index < location.size())return std::nullopt;
    return {*current};
}