#ifndef MN_CFG
#define MN_CFG
#include <arpa/inet.h>
#include <string_view>
#include <vector>
#include <memory_resource>
#include <string>
#include <ranges>
#include <string.h>
#include <iostream>

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