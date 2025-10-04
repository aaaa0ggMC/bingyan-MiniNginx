#ifndef MN_CFG
#define MN_CFG
#include <arpa/inet.h>
#include <string_view>
#include <vector>
#include <memory_resource>
#include <string>
#include <ranges>
#include <string.h>

namespace mnginx{
    struct Config{
        sockaddr_in server_socket;
        sockaddr_in revproxy_socket;

        struct Node{
            std::pmr::string name;
            std::vector<std::pmr::string> values;
            
            std::vector<Node> children;

            std::optional<std::reference_wrapper<Config::Node>> get_node_recursive(const std::vector<std::string_view> & location,unsigned int expected_index = 0);

            inline auto get_child_nodes_view(const std::string& name){
                return children | std::views::filter([name](const Node & node){
                    bool match = !strcmp(name.c_str(),node.name.c_str());
                    return match;
                });
            }

            inline std::optional<std::string_view> get_node_recursive_value(const std::vector<std::string_view> & location,unsigned int expected_index = 0,unsigned int value_index = 0){
                auto node = get_node_recursive(location,expected_index);
                if(node && node->get().values.size() > value_index){
                    auto & str = node->get().values[value_index];
                    if(str.size() > 2 && str[0] == '\"')return std::string_view(str.begin()+1,str.end()-1);
                    else if(str.size() == 2 && str[0] == str[1] && str[0] == '\"')return "";
                    return str;
                }else return std::nullopt;
            }
        };

        enum class LoadResult{
            OK,
            EOFTooEarly,
            WrongGrammar
        };

        Node config_nodes;

        inline Config(){}

        LoadResult load_from_buffer(std::string_view buffer);

    private:
        LoadResult analyse_words(std::vector<std::string> & tokens);
    };
}

#endif