#include <config.h>
#include <gtest/gtest.h>

using namespace mnginx;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
    
    void printNode(const Config::Node& node, int depth = 0) {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "Node: \"" << node.name << "\"";
        if (!node.values.empty()) {
            std::cout << " [Values: ";
            for (size_t i = 0; i < node.values.size(); ++i) {
                std::cout << "\"" << node.values[i] << "\"";
                if (i < node.values.size() - 1) std::cout << ", ";
            }
            std::cout << "]";
        }
        std::cout << std::endl;
        for (const auto& child : node.children) {
            printNode(child, depth + 1);
        }
    }
};

TEST_F(ConfigTest, CustomConfigTest) {
    Config config;
    std::string test_config = R"(server{ a b; a c ; proxy "{"; } client{shit a;})";  // 你自己填
    
    auto result = config.load_from_buffer(test_config);
    
    std::cout << "=== Config Test ===" << std::endl;
    std::cout << "Load Result: " << static_cast<int>(result) << std::endl;
    
    if (result == Config::LoadResult::OK) {
        for (const auto& child : config.config_nodes.children) {
            printNode(child);
        }
    }

    std::cout << "=== Get Node Test ===" << std::endl;
    auto re = config.config_nodes.get_node_recursive({"server","a"});
    EXPECT_NE(re,std::nullopt);
    EXPECT_EQ(re->get().values[0],"b");
    re = config.config_nodes.get_node_recursive({"server","a"},1);
    EXPECT_NE(re,std::nullopt);
    EXPECT_EQ(re->get().values[0],"c");
    re = config.config_nodes.get_node_recursive({"server"},100);
    EXPECT_NE(re,std::nullopt);
    EXPECT_EQ(re->get().children.size(),3);
    for(auto & node : re->get().get_child_nodes_view("a")){
        printNode(node);
    }
}