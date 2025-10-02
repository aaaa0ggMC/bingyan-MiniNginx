#include <application.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stack>
#include <gtest/gtest.h>
#include <http_parser.h>

using namespace mnginx;

Application::Application():logger{LOG_SHOW_HEAD | LOG_SHOW_THID | LOG_SHOW_TIME | LOG_SHOW_TYPE},lg{"MiniNginx",logger}{
    server_fd = -1;
}

Application::~Application(){
    if(server_fd != -1)close(server_fd);
}

//// Setup Section ////
void Application::setup(){
    setup_general();
    setup_logger();

    setup_handlers();
    setup_server();
}

void Application::setup_general(){
    // Initialize cpp pmr resource pool,note that I'm not very familiar with it
    // I just know how to use
    resource = &pool;
    std::pmr::set_default_resource(resource);
}

void Application::setup_server(){
    int len = sizeof(address);

    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){
        lg(LOG_CRITI) << "Cannot create a socket for the server:" << strerror(errno) << endlog;
        std::exit(-1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(9191);

    // non-block mode
    fcntl(server_fd,F_SETFL,fcntl(server_fd,F_GETFL,0) | O_NONBLOCK);

    // resuable port
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        lg(LOG_WARN) << "Cannot set SO_REUSEADDR:" << strerror(errno) << endlog;
    }

    if(bind(server_fd,(sockaddr*)&address,sizeof(address)) == -1){
        lg(LOG_CRITI) << "Cannot bind the port " << ntohs(address.sin_port) << " for the server:" << strerror(errno) << endlog;
        std::exit(-1);
    }

    // n is the count of backlog,other clients will be suspended
    if(listen(server_fd,1024) == -1){
        lg(LOG_CRITI) << "Cannot listen the port " << ntohs(address.sin_port) << ":" << strerror(errno) << endlog;
        std::exit(-1);
    }
    
    // Epoll settings
    epoll.resize(1024);

    // listen for connections
    epoll.add(server_fd,EPOLLIN); // care about read ==> client connections,no EPOLLET,this will damage the connection

    /// all set,now let's wait for client in the run function 
}

void Application::setup_logger(){
    logger.appendLogOutputTarget("file",std::make_shared<lot::SplittedFiles>("./data/latest.log",4 * 1024 * 1024));
    logger.appendLogOutputTarget("console",std::make_shared<lot::Console>());
}

void Application::setup_handlers(){
    StateNode file;
    file.node(HandlerRule::Match_Any);
    handlers.add_new_handler(file, [](HTTPRequest & rq, const std::pmr::vector<std::pmr::string>& vals, HTTPResponse& rp){
        rp.status_code = HTTPResponse::StatusCode::OK;
        rp.status_str = "OK";
        rp.headers["Content-Type"] = "text/html; charset=utf-8";
        rp.headers["Connection"] = "close";
        
        std::string first_val = vals.empty() ? "No Value" : std::string(vals[0].data(), vals[0].size());
        std::string html = "<html><body><h1>Hello World</h1><p>Value: " + first_val + "</p></body></html>";
        
        // 直接构造 HTTPData (pmr::vector<char>)
        rp.data.emplace(html.begin(), html.end());
        rp.headers[KEY_Content_Length] = std::to_string(rp.data->size());
        
        return HandleResult::Continue;
    });
    file = StateNode();
    file.node("file").node(HandlerRule::Match_Any);
    handlers.add_new_handler(file, [](HTTPRequest & rq, const std::pmr::vector<std::pmr::string>& vals, HTTPResponse& rp){
        rp.status_code = HTTPResponse::StatusCode::OK;
        rp.status_str = "OK";
        rp.headers["Content-Type"] = "text/plain; charset=utf-8";
        rp.headers["Connection"] = "close";
        
        // let's read
        std::string ou = "";
        std::string path ="/sorry/path/encrypted/";
        path += vals[1];
        alib::g3::Util::io_readAll(path,ou);
        
        std::cout << "PATH:" << path << std::endl;

        // 直接构造 HTTPData (pmr::vector<char>)
        rp.data.emplace(ou.begin(), ou.end());
        
        return HandleResult::Continue;
    });
}

//// Main Section ////
void Application::run(){
    while(true){
        auto [ok,ev_view] = epoll.wait(4000);
        if(ok){
            if(ev_view.size()){
                lg(LOG_INFO) << "Received events!" << endlog;
                for(auto & ev : ev_view){
                    if(ev.data.fd == server_fd){
                        accept_connections();
                        continue;
                    }
                    handle_client(ev);
                }
            }else{ // timeout
                // lg(LOG_WARN) << "Waited to timeout but received no clients!" << endlog;
            }
        }else{
            lg(LOG_ERROR) << "Error occured when waiting for messages:" << strerror(errno) << endlog;
        }
    }
}

void Application::accept_connections(){
    int client_fd = -1;
    struct sockaddr_in client_info;
    int sizelen = sizeof(sockaddr_in);
    while(true){
        if((client_fd = accept(server_fd,(sockaddr*)&client_info,(socklen_t*)&sizelen)) == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                lg(LOG_TRACE) << "Accept work finished." << endlog;
                break;
            }else lg(LOG_ERROR) << "Error occured when accepting new client!:" << strerror(errno) << endlog;
        }else{
            fcntl(client_fd,F_SETFL,fcntl(client_fd,F_GETFL,0) | O_NONBLOCK);
            lg(LOG_INFO) << "Established new client: fd[" << client_fd << "] addr[" << client_info.sin_addr.s_addr << "]" << endlog;
            establishedClients.emplace(client_fd,ClientInfo());

            // add epoll events
            epoll.add(client_fd,EPOLLIN | EPOLLET);
        }
    }
}

void Application::handle_client(epoll_event & ev){
    thread_local static char buffer[1024]; // thread_local is just a decorative item I think
    int client_fd = ev.data.fd;
    bool shouldClose = false;

    if(!(ev.events & EPOLLIN))return;
    
    if(ev.events & EPOLLRDHUP){
        lg(LOG_INFO) << "Client " << client_fd << " performed orderly shutdown" << endlog;
        shouldClose = true;
    }
    
    if(ev.events & EPOLLHUP){
        lg(LOG_INFO) << "Client " << client_fd << " hung up" << endlog;
        shouldClose = true;
    }
    
    if(ev.events & EPOLLERR){
        lg(LOG_INFO) << "Client " << client_fd << " connection error" << endlog;
        shouldClose = true;
    }

    if(shouldClose){
        cleanup_connection(client_fd);
        return;
    }

    // @todo remember to add while(1) and write it into buffer
    auto it = establishedClients.find(client_fd);
    if(it == establishedClients.end()){ // no way!
        // i think there's no way this code could be run
        lg(LOG_ERROR) << "The coming client is not recorded!" << endlog; 
        cleanup_connection(client_fd);
        return;
    }
    do{
        size_t count = read(client_fd, buffer, sizeof(buffer));
        if(count == 0){
            lg(LOG_INFO) << "Client closed the connection" << endlog;
            shouldClose = true;
        }else if(count == -1 && errno != EAGAIN){
            lg(LOG_ERROR) << "Error occured when reading data from client:" << strerror(errno) << endlog;
            shouldClose = true;
        }else if(count > 0){
            std::string test(buffer,buffer + count);
            lg(LOG_INFO) << "Received \"" << test << " \"from client" << endlog;
            it->second.buffer.insert(it->second.buffer.end(),buffer,buffer + count);
        }
        if(count == sizeof(buffer))continue; //maybe there's more data
    }while(false);

    // @todo if the size of the buffer exceedes 8MB(can be configured later),we treat it as an sus one
    if(it->second.buffer.size() > 8 * 1024 * 1024){
        lg(LOG_ERROR) << "The client fd[" << client_fd << "] post toos long a message!" << endlog;
        shouldClose = true;
    }

    if(shouldClose){
        cleanup_connection(client_fd);
    }else{
        // try to unpack the buffer and extract data
        std::string_view buffer_str (it->second.buffer.begin() + it->second.find_last_pos,it->second.buffer.end());
        if(it->second.pending){
            // @todo WIP
            handle_pending_request(it->second,client_fd);
            //maybe we can deal another message then,so no return here
        }

        auto pos = buffer_str.find("\r\n\r\n");

        if(pos != std::string_view::npos){
            pos += it->second.find_last_pos + 4;
            // We dont care about CLEN or OTHERS,now we just analyse the header
            // the first of the request begins with an alphabet,so we ignore others
            int i = 0;
            std::string_view data_chunk (it->second.buffer.begin(),it->second.buffer.begin() + pos);
            for(;(data_chunk[i] < 'A' || data_chunk[i] > 'Z') && i < data_chunk.size();++i)continue;
            if(i >= data_chunk.size()){
                // invalid connection data,so we close the connection
                cleanup_connection(client_fd);
                return;
            }
            std::string_view valid_chunk (it->second.buffer.begin() + i,it->second.buffer.begin() + pos);
            it->second.pending_request = HTTPRequest();
            lg(LOG_INFO) <<  "We have processed:["  << std::string(valid_chunk.begin(),valid_chunk.end()) << "]" << std::endl;
            if(it->second.pending_request.parse(valid_chunk) != ParseCode::Success){
                // invalid HTTP Method
                cleanup_connection(client_fd);
                return;
            }else{
                lg(LOG_TRACE) << "Successfully parsed a header: method[" <<
                HTTPRequest::getMethodString(it->second.pending_request.method) << "] url[" <<
                it->second.pending_request.url.full_path() << "] version[" << it->second.pending_request.version.major << "."
                << it->second.pending_request.version.minor << "]..." << endlog; 
            }
            // note that all the string_view will have memeory issue after this call
            it->second.buffer.erase(it->second.buffer.begin(),it->second.buffer.begin() + pos); 
            it->second.find_last_pos = 0;
            it->second.pending = true;
            // some simple methods(GET) have no message body
            handle_pending_request(it->second,client_fd);
        }else{
            it->second.find_last_pos = it->second.buffer.size();
        }
    }
}

bool Application::cleanup_connection(int fd){
    auto it = establishedClients.find(fd);
    if(it == establishedClients.end()){
        lg(LOG_WARN) << "Trying to cleanup a fd that is not recorded." << endlog;
        return false;
    }else{
        // cleanup epoll data
        epoll.del(fd);
        close(fd); // close client fd from server
        establishedClients.erase(it);
        lg(LOG_INFO) << "Client with fd [" << fd << "] was closed." << endlog;
        return true;
    }
}

void Application::handle_pending_request(ClientInfo & client,int fd){
    auto & req = client.pending_request;
    thread_local static HTTPResponse response = []{
        HTTPResponse resp;
        resp.data.emplace();
        resp.version.major = 1;
        resp.version.minor = 1;
        return resp;
    }();
    using M = HTTPRequest::HTTPMethod;

    switch(req.method){
    case M::GET:{
        if(req.headers.find(KEY_Content_Length) != req.headers.end() ||
           req.headers.find(KEY_Transfer_Encoding) != req.headers.end()){ //bad request
            lg(LOG_ERROR) << "Client " << fd << " passed GET request with message body!" << endlog;
            send_message_simp(fd,HTTPResponse::StatusCode::BadRequest,
                HTTPResponse::get_status_description(HTTPResponse::StatusCode::BadRequest));
            cleanup_connection(fd);
            return;
        }else{ //nice guys now we can send something useful back
            HandlerFn fn;
            std::pmr::vector<std::pmr::string> vals;
            auto result = handlers.parseURL(client.pending_request.url.main_path(),fn,vals);
            if(result == StateTree::ParseResult::OK){
                response.reset();
                fn(client.pending_request,vals,response);
                send_message(fd,response);
            }else{
                lg(LOG_INFO) << "Found nothing for the client " << (int)result << endlog;
                // @todo add a not found handler!
                send_message_simp(fd,HTTPResponse::StatusCode::NotFound,"Not Found!");
            }
        }
        client.pending = false;
        break;
    }
    case M::POST:{ // there's data in the message body,so we need to extract the data
        bool ok = false;
        auto p = req.headers.find(KEY_Transfer_Encoding);
        if(!client.pending_request.data)client.pending_request.data.emplace();
        if(p != req.headers.end()){
            if(p->second.compare("chunked")){
                lg(LOG_ERROR) << "Transfer-Encoding mode "  << p->second <<  " is not supported." << std::endl;
                return;
            }
            // chunked mode
            // read existing data
            while(true){
                std::string_view buffer (client.buffer.begin(),client.buffer.end());
                char * endptr = nullptr;
                size_t pos = buffer.find_first_of("\r\n");
                if(pos == std::string_view::npos)return; // data is not well prepared
                int count = strtol(buffer.data(),&endptr,16); // @note the chunked data is a hex number
                if(endptr == buffer.data()){ // invalid character
                        lg(LOG_ERROR) << "Client " << fd << " passed POST request with bad message body!" << endlog;
                        send_message_simp(fd,HTTPResponse::StatusCode::BadRequest,
                            HTTPResponse::get_status_description(HTTPResponse::StatusCode::BadRequest));
                        cleanup_connection(fd);
                }
                if(((int)(buffer.size()) - 4 - count - pos) >= 0){ // why minus 4? ==> two \r\n
                    if(count)client.pending_request.data->insert(client.pending_request.data->end(),
                            buffer.begin() + (pos+2),buffer.begin() + (pos+2+count));
                    // check whether it ends with \r\n]
                    if(strncmp("\r\n",buffer.data() + (pos+2+count),2)){
                        lg(LOG_ERROR) << "Client " << fd << " passed POST request with bad message body!" << endlog;
                        send_message_simp(fd,HTTPResponse::StatusCode::BadRequest,
                            HTTPResponse::get_status_description(HTTPResponse::StatusCode::BadRequest));
                        cleanup_connection(fd);
                        return;
                    }
                    // update buffer
                    client.buffer.erase(client.buffer.begin(),client.buffer.begin() + (pos+4+count));
                    if(count == 0){
                        // that means the chunk ended
                        ok = true;
                        break;
                    }
                }else return;
            }
        }
        if(!ok){
            p = req.headers.find(KEY_Content_Length);
            if(p != req.headers.end()){
                char * endptr = nullptr;
                int length = strtol(p->second.data(),&endptr,10);
                if(endptr != p->second.data()){ // actually have data here!
                    if(client.buffer.size() < length){
                        return; // data not well prepared
                    }
                    lg(LOG_WARN) << "Data Well Prepared!" << endlog;
                    client.pending_request.data->insert(
                        client.pending_request.data->end(),
                        client.buffer.begin(),
                        client.buffer.begin() + length
                    );
                    // update buffer
                    client.buffer.erase(client.buffer.begin(),client.buffer.begin() + length);
                }// else just ignore,anyway,the task is not pending now
            }
        }
        // @todo ... add code here
        client.pending = false;

        HandlerFn fn;
        std::pmr::vector<std::pmr::string> vals;
        auto result = handlers.parseURL(client.pending_request.url.main_path(),fn,vals);
        if(result == StateTree::ParseResult::OK){
            response.reset();
            fn(client.pending_request,vals,response);
            send_message(fd,response);
        }else{
            lg(LOG_INFO) << "Found nothing for the client" << endlog;
            send_message_simp(fd,HTTPResponse::StatusCode::NotFound,"Not Found!");
        }
        break;
    }
    default: //no way!
        break;
    }
}

size_t Application::send_message_simp(int fd,HTTPResponse::StatusCode code,std::string_view stat_str){
    thread_local static HTTPResponse resp;
    resp.version.major = 1;
    resp.version.minor = 1;
    resp.status_code = code;
    resp.status_str.assign(stat_str.begin(),stat_str.end());
    // checkout will auto do this
    // resp.headers[KEY_Content_Length] = "0";

    return send_message(fd,resp);
}

size_t Application::send_message(int fd,HTTPResponse & resp,HTTPResponse::TransferMode mode){
    // auto checkout
    resp.checkout_data(mode);
    auto data = resp.generate();
    return send(fd,data.data(),data.size(),0);
}

void StateTree::clear_tree(){
    root = Node();
    // the code below is not that essential too
    root.rule = HandlerRule::FixedString;
    root.data_str = "/";
}

StateTree::AddResult StateTree::add_new_handler(StateNode& tree,HandlerFn handler){
    // step 1: check whether the tree has an handler
    if(!handler)return AddResult::NoHandler;
    Node * current = &root;
    StateNode * pnode = &tree;
    
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

StateTree::ParseResult StateTree::parseURL(std::string_view main_path,HandlerFn & fn,std::pmr::vector<std::pmr::string> & node_vals){
    if(main_path.size() < 1)return ParseResult::EmptyURL;
    if(main_path[0] == '/')main_path = main_path.substr(1); // we leave space for path with no '/' prefix
    Node * current = &root;
    size_t pos = 0;

    node_vals.clear();

    while(true){
        std::string_view name = "";
        Node * match_any = nullptr;
        bool find_ok = false;
        bool no_child = true;

        pos = main_path.find_first_of('/');
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
                        return ParseResult::OK;
                    }else{
                        return ParseResult::Node404;
                    }
                }
                current = &n;
                find_ok = true;
            }
        }
        
        if(!find_ok){
            if(match_any){
                current = match_any;
            }else if(no_child && current->nexts.size() == 1){ // that means we stepped into the end
                if(current->nexts[0].handler){
                    fn = current->nexts[0].handler;
                    if(node_vals.size() >= 1){
                        std::pmr::string & last = node_vals[node_vals.size()-1];
                        last.append("/");
                        last.append(main_path.begin(),main_path.end());
                        std::cout << "child:" << last << std::endl;
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

#ifdef MNGINX_TESTING
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
#endif
