#include <gtest/gtest.h>
#include <application.h>

using namespace mnginx;

TEST(StateTreeTest, BuildAndTraverseTree){
    StateTree state_tree;
    
    // 构建一个测试用的状态树
    StateNode tree;
    
    // 构建路径: node1 -> node2 -> node3
    tree.node("node1")
       .node("node2")
       .node("node3");
    
    // 添加处理器
    auto handler1 = [](HTTPRequest &,const std::pmr::vector<std::pmr::string>&,HTTPResponse&){
         std::cout << "Handler 1 called" << std::endl; 
         return HandleResult::Continue;
        };
    std::cout << "A" << std::endl;
    state_tree.add_new_handler(tree, handler1);
    std::cout << "B" << std::endl;
    
    // 构建另一条路径: any_rule -> node4
    StateNode tree2;
    tree2.node(HandlerRule::Match_Any)
        .node("node4");
    
    auto handler2 = [](HTTPRequest &,const std::pmr::vector<std::pmr::string>&,HTTPResponse&){
         std::cout << "Handler 2 called" << std::endl; 
         return HandleResult::Continue;
        };
    std::cout << "C" << std::endl;
    state_tree.add_new_handler(tree2, handler2);
    std::cout << "D" << std::endl;
    
    // 构建第三条路径: node5 -> any_rule
    StateNode tree3;
    tree3.node("node5")
        .node(HandlerRule::Match_Any);
    
    auto handler3 = [](HTTPRequest &,const std::pmr::vector<std::pmr::string>&,HTTPResponse&){
         std::cout << "Handler 3 called" << std::endl; 
         return HandleResult::Continue;
        };
    std::cout << "E" << std::endl;
    state_tree.add_new_handler(tree3, handler3);
    std::cout << "F" << std::endl;
    
    // 打印整个状态树结构
    std::cout << "\n=== State Tree Structure ===" << std::endl;
    
    std::function<void(const StateTree::Node&, int)> print_node = [&](const StateTree::Node& node, int depth) {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "Node: ";
        
        if (node.rule == HandlerRule::Match_Any) {
            std::cout << "[Match_Any]";
        } else {
            std::cout << "\"" << node.data_str << "\"";
        }
        
        if (node.handler) {
            std::cout << " [HAS_HANDLER]";
        }
        
        std::cout << " (children: " << node.nexts.size() << ")" << std::endl;
        
        for (const auto& child : node.nexts) {
            print_node(child, depth + 1);
        }
    };
    
    print_node(state_tree.root, 0);
    std::cout << "=== End of State Tree ===" << std::endl;
    
    // 打印原始StateNode树结构
    std::cout << "\n=== Original StateNode Trees ===" << std::endl;
    
    std::function<void(const StateNode&, int)> print_state_node = [&](const StateNode& node, int depth) {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "StateNode: ";
        
        if (node.rule == HandlerRule::Match_Any) {
            std::cout << "[Match_Any]";
        } else {
            std::cout << "\"" << node.data_str << "\"";
        }
        
        std::cout << " (empty: " << node.empty << ")";
        
        if (node.next) {
            std::cout << " ->" << std::endl;
            print_state_node(*(node.next), depth + 1);
        } else {
            std::cout << " [END]" << std::endl;
        }
    };
    
    std::cout << "Tree 1:" << std::endl;
    print_state_node(tree, 1);
    
    std::cout << "\nTree 2:" << std::endl;
    print_state_node(tree2, 1);
    
    std::cout << "\nTree 3:" << std::endl;
    print_state_node(tree3, 1);
    
    std::cout << "=== End of StateNode Trees ===" << std::endl;
}

TEST(StateTreeTest, EmptyTreeTest) {
    StateTree state_tree;
    StateNode empty_tree; // 空的树
    
    auto handler = [](HTTPRequest &,const std::pmr::vector<std::pmr::string>&,HTTPResponse&){
         std::cout << "EMpty tree handler." << std::endl; 
         return HandleResult::Continue;
        };
    auto result = state_tree.add_new_handler(empty_tree, handler);
    
    std::cout << "\n=== Empty Tree Test ===" << std::endl;
    std::cout << "Add result: " << static_cast<int32_t>(result) << std::endl;
    
    // 打印结果树
    std::function<void(const StateTree::Node&, int)> print_node = [&](const StateTree::Node& node, int depth) {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "Node: ";
        
        if (node.rule == HandlerRule::Match_Any) {
            std::cout << "[Match_Any]";
        } else {
            std::cout << "\"" << node.data_str << "\"";
        }
        
        if (node.handler) {
            std::cout << " [HAS_HANDLER]";
        }
        
        std::cout << " (children: " << node.nexts.size() << ")" << std::endl;
        
        for (const auto& child : node.nexts) {
            print_node(child, depth + 1);
        }
    };
    
    print_node(state_tree.root, 0);
    std::cout << "=== End of Empty Tree Test ===" << std::endl;
}

#include <gtest/gtest.h>

TEST(StateTreeStructureTest, BasicPathConstruction) {
    StateTree state_tree;
    
    // 测试基本路径构建
    StateNode tree;
    tree.node("api").node("v1").node("users");
    
    auto handler = [](HTTPRequest&, const std::pmr::vector<std::pmr::string>&, HTTPResponse&) {
        return HandleResult::Continue;
    };
    
    auto result = state_tree.add_new_handler(tree, handler);
    EXPECT_EQ(result, StateTree::AddResult::Success);
    
    // 验证树结构（通过遍历打印）
    std::cout << "=== Basic Path Structure ===" << std::endl;
    // 可以添加结构验证的辅助函数
}

TEST(StateTreeStructureTest, WildcardPathConstruction) {
    StateTree state_tree;
    
    // 测试通配符路径构建
    StateNode tree;
    tree.node(HandlerRule::Match_Any).node("profile");
    
    auto handler = [](HTTPRequest&, const std::pmr::vector<std::pmr::string>&, HTTPResponse&) {
        return HandleResult::Continue;
    };
    
    auto result = state_tree.add_new_handler(tree, handler);
    EXPECT_EQ(result, StateTree::AddResult::Success);
    
    // 验证通配符节点正确创建
}

TEST(StateTreeStructureTest, ConflictDetection) {
    StateTree state_tree;
    
    StateNode tree1;
    tree1.node("users").node("123");
    
    StateNode tree2;  
    tree2.node("users").node("123"); // 相同路径
    
    auto handler1 = [](HTTPRequest&, const std::pmr::vector<std::pmr::string>&, HTTPResponse&) {
        return HandleResult::Continue;
    };
    
    auto handler2 = [](HTTPRequest&, const std::pmr::vector<std::pmr::string>&, HTTPResponse&) {
        return HandleResult::Close;
    };
    
    EXPECT_EQ(state_tree.add_new_handler(tree1, handler1), StateTree::AddResult::Success);
    EXPECT_EQ(state_tree.add_new_handler(tree2, handler2), StateTree::AddResult::ConflictHandler);
}

TEST(StateTreeStructureTest, NoHandlerValidation) {
    StateTree state_tree;
    StateNode tree;
    tree.node("test");
    
    HandlerFn null_handler;
    auto result = state_tree.add_new_handler(tree, null_handler);
    EXPECT_EQ(result, StateTree::AddResult::NoHandler);
}

TEST(StateTreeStructureTest, DeepNestingLimit) {
    StateTree state_tree;
    
    // 测试深层嵌套
    StateNode deep_tree;
    StateNode* current = &deep_tree;
    for (int i = 0; i < 10; ++i) {
        current = &current->node("level" + std::to_string(i));
    }
    
    auto handler = [](HTTPRequest&, const std::pmr::vector<std::pmr::string>&, HTTPResponse&) {
        return HandleResult::Continue;
    };
    
    auto result = state_tree.add_new_handler(deep_tree, handler);
    EXPECT_EQ(result, StateTree::AddResult::Success);
}

TEST(StateTreeStructureTest, MixedRulesConstruction) {
    StateTree state_tree;
    
    // 测试混合规则
    StateNode mixed_tree;
    mixed_tree.node("api")                    // 精确匹配
              .node(HandlerRule::Match_Any)   // 通配符
              .node("v1")                     // 精确匹配  
              .node(HandlerRule::Match_Any);  // 通配符
    
    auto handler = [](HTTPRequest&, const std::pmr::vector<std::pmr::string>&, HTTPResponse&) {
        return HandleResult::Continue;
    };
    
    auto result = state_tree.add_new_handler(mixed_tree, handler);
    EXPECT_EQ(result, StateTree::AddResult::Success);
}

// 辅助函数：验证树结构一致性
void ValidateTreeStructure(const StateTree::Node& node, int depth = 0) {
    std::string indent(depth * 2, ' ');
    std::cout << indent << "Node: ";
    
    if (node.rule == HandlerRule::Match_Any) {
        std::cout << "[Match_Any]";
    } else {
        std::cout << "\"" << node.data_str << "\"";
    }
    
    if (node.handler) {
        std::cout << " [HAS_HANDLER]";
    }
    
    std::cout << " (children: " << node.nexts.size() << ")" << std::endl;
    
    for (const auto& child : node.nexts) {
        ValidateTreeStructure(child, depth + 1);
    }
}

TEST(StateTreeStructureTest, StructureConsistency) {
    StateTree state_tree;
    
    // 构建复杂树结构
    StateNode tree1, tree2, tree3;
    tree1.node("a").node("b");
    tree2.node("a").node("c"); 
    tree3.node("x").node(HandlerRule::Match_Any);
    
    auto handler = [](auto&&...){ return HandleResult::Continue; };
    
    state_tree.add_new_handler(tree1, handler);
    state_tree.add_new_handler(tree2, handler); 
    state_tree.add_new_handler(tree3, handler);
    
    std::cout << "=== Final Tree Structure ===" << std::endl;
    ValidateTreeStructure(state_tree.root);
}
TEST(StateTreeParseTest, WildcardPathParsing) {
    StateTree state_tree;
    
    // 构建路径: /api/{ANY}/say
    StateNode tree;
    tree.node("api")
       .node(HandlerRule::Match_Any)  // {ANY} 通配符
       .node("say");
    
    // 创建处理器，打印通配符值
    auto handler = [](HTTPRequest& req, const std::pmr::vector<std::pmr::string>& params, HTTPResponse& res) {
        std::cout << "Handler called with params: ";
        for (const auto& param : params) {
            std::cout << "[" << param << "] ";
        }
        std::cout << std::endl;
        return HandleResult::Continue;
    };
    
    // 添加处理器到状态树
    auto add_result = state_tree.add_new_handler(tree, handler);
    EXPECT_EQ(add_result, StateTree::AddResult::Success);
    
    // 测试路径解析
    HandlerFn found_handler;
    std::pmr::vector<std::pmr::string> node_vals;
    
    // 测试用例1: /api/hello/say
    std::cout << "\n=== Testing /api/hello/say ===" << std::endl;
    auto result1 = state_tree.parseURL("/api/hello/say", found_handler, node_vals);
    
    std::cout << "Parse result: " << static_cast<int>(result1) << std::endl;
    std::cout << "Node values captured: ";
    for (const auto& val : node_vals) {
        std::cout << "[" << val << "] ";
    }
    std::cout << std::endl;
    std::cout << "Handler found: " << (found_handler != nullptr) << std::endl;
    
    // 测试用例2: /api/world/say  
    std::cout << "\n=== Testing /api/world/say ===" << std::endl;
    auto result2 = state_tree.parseURL("/api/world/say", found_handler, node_vals);
    
    std::cout << "Parse result: " << static_cast<int>(result2) << std::endl;
    std::cout << "Node values captured: ";
    for (const auto& val : node_vals) {
        std::cout << "[" << val << "] ";
    }
    std::cout << std::endl;
    std::cout << "Handler found: " << (found_handler != nullptr) << std::endl;
    
    // 如果找到处理器，调用它来验证参数
    if (found_handler) {
        HTTPRequest dummy_req;
        HTTPResponse dummy_res;
        auto handle_result = found_handler(dummy_req, node_vals, dummy_res);
        std::cout << "Handler returned: " << static_cast<int>(handle_result) << std::endl;
    }
    
    // 打印状态树结构用于调试
    std::cout << "\n=== State Tree Structure ===" << std::endl;
    std::function<void(const StateTree::Node&, int)> print_node = [&](const StateTree::Node& node, int depth) {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "Node: ";
        
        if (node.rule == HandlerRule::Match_Any) {
            std::cout << "[Match_Any]";
        } else {
            std::cout << "\"" << node.data_str << "\"";
        }
        
        if (node.handler) {
            std::cout << " [HAS_HANDLER]";
        }
        
        std::cout << " (children: " << node.nexts.size() << ")" << std::endl;
        
        for (const auto& child : node.nexts) {
            print_node(child, depth + 1);
        }
    };
    print_node(state_tree.root, 0);
}
TEST(StateTreeParseTest, MultipleTreesRealWorldScenario) {
    // 创建多个独立的状态树，模拟不同的路由模块
    StateTree api_tree;    // API 路由
    StateTree auth_tree;   // 认证路由  
    StateTree static_tree; // 静态资源路由

    // === 1. 设置 API 路由树 ===
    {
        // /api/users/{id}/profile
        StateNode api_route;
        api_route.node("api").node("users").node(HandlerRule::Match_Any).node("profile");
        
        auto api_handler = [](HTTPRequest& req, const std::pmr::vector<std::pmr::string>& params, HTTPResponse& res) {
            std::cout << "API Handler: User profile for ID: " << params[2] << std::endl;
            // params: [api, users, {user_id}, profile]
            return HandleResult::Close;
        };
        
        auto result = api_tree.add_new_handler(api_route, api_handler);
        EXPECT_EQ(result, StateTree::AddResult::Success);
    }

    // === 2. 设置认证路由树 ===
    {
        // /auth/{provider}/callback
        StateNode auth_route;
        auth_route.node("auth").node(HandlerRule::Match_Any).node("callback");
        
        auto auth_handler = [](HTTPRequest& req, const std::pmr::vector<std::pmr::string>& params, HTTPResponse& res) {
            std::cout << "Auth Handler: OAuth callback for provider: " << params[1] << std::endl;
            // params: [auth, {provider}, callback]
            return HandleResult::Close;
        };
        
        auto result = auth_tree.add_new_handler(auth_route, auth_handler);
        EXPECT_EQ(result, StateTree::AddResult::Success);
    }

    // === 3. 设置静态资源路由树 ===
    {
        // /static/{category}/{filename}
        StateNode static_route;
        static_route.node("static").node(HandlerRule::Match_Any).node(HandlerRule::Match_Any);
        
        auto static_handler = [](HTTPRequest& req, const std::pmr::vector<std::pmr::string>& params, HTTPResponse& res) {
            std::cout << "Static Handler: Serving file: " << params[1] << "/" << params[2] << std::endl;
            // params: [static, {category}, {filename}]
            return HandleResult::Close;
        };
        
        auto result = static_tree.add_new_handler(static_route, static_handler);
        EXPECT_EQ(result, StateTree::AddResult::Success);
    }

    // === 测试路由分发 ===
    std::vector<std::pair<std::string, StateTree*>> test_cases = {
        {"/api/users/12345/profile", &api_tree},
        {"/api/users/67890/profile", &api_tree},
        {"/auth/google/callback", &auth_tree},
        {"/auth/github/callback", &auth_tree},
        {"/static/images/logo.png", &static_tree},
        {"/static/css/style.css", &static_tree},
        {"/api/products/999/details", &api_tree}  // 这个应该404
    };

    for (auto& [url, tree] : test_cases) {
        std::cout << "\n=== Testing: " << url << " ===" << std::endl;
        
        HandlerFn found_handler;
        std::pmr::vector<std::pmr::string> node_vals;
        
        auto result = tree->parseURL(url, found_handler, node_vals);
        
        std::cout << "Parse result: " << static_cast<int>(result) << std::endl;
        std::cout << "Path segments: ";
        for (size_t i = 0; i < node_vals.size(); ++i) {
            std::cout << "[" << i << "]:\"" << node_vals[i] << "\" ";
        }
        std::cout << std::endl;
        
        if (result == StateTree::ParseResult::OK && found_handler) {
            HTTPRequest dummy_req;
            HTTPResponse dummy_res;
            auto handle_result = found_handler(dummy_req, node_vals, dummy_res);
            std::cout << "Handler execution result: " << static_cast<int>(handle_result) << std::endl;
        } else {
            std::cout << "No handler found or parse failed" << std::endl;
        }
    }

    // === 打印所有树结构 ===
    std::cout << "\n=== API Tree Structure ===" << std::endl;
    std::function<void(const StateTree::Node&, int)> print_node = [&](const StateTree::Node& node, int depth) {
        std::string indent(depth * 2, ' ');
        std::cout << indent;
        
        if (node.rule == HandlerRule::Match_Any) {
            std::cout << "[Match_Any]";
        } else {
            std::cout << "\"" << node.data_str << "\"";
        }
        
        if (node.handler) std::cout << " [HANDLER]";
        std::cout << std::endl;
        
        for (const auto& child : node.nexts) {
            print_node(child, depth + 1);
        }
    };

    std::cout << "API Tree:" << std::endl;
    print_node(api_tree.root, 1);
    
    std::cout << "\nAuth Tree:" << std::endl;
    print_node(auth_tree.root, 1);
    
    std::cout << "\nStatic Tree:" << std::endl;
    print_node(static_tree.root, 1);
}