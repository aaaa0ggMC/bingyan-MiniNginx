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
#include <route_states.h>
#include <modules/module_base.h>
#include <client.h>
#include <server.h>
#include <epoll.h>
#include <config.h>

// Since my library is verbose,so here I globally use the namespace
using namespace alib::g3;

namespace mnginx{
    template<typename T>
    concept HasModuleHandler = requires(T mod,HandlerContext ctx) {
        { T::handle(ctx) } -> std::same_as<HandleResult>;
    };

    /**
     * @brief The main entry of MiniNginx
     * @start-date 2025/10/02
     */
    class Application{
    private:
        //// Log System
        Logger logger; ///< access logger
        LogFactory lg; ///< the log factory of access logger
        Logger lgerr; ///< error logger
        LogFactory lge; ///< the log factory of error logger
        
        //// Memory Management
        /// memory pool that takes advantages of huge memory block
        std::pmr::synchronized_pool_resource pool; 

        //// Servers
        /// The main server,will upgrade to master-worker threaded mode in the future
        std::unique_ptr<Server> server; 
        
        //// Route System
        /**
         * @brief a tree state machine that analysis the url and call related handlers,
         * it's fragile so make sure your registration is nice
         */
        StateTree handlers;

        /// modules
        modules::ModuleFuncs mods;

        /// config system
        Config config;
    public:
        /// used in main() to return,initially 0
        int return_result; 

        /// initializes logger,no output target now
        Application();

        //// Setup Section ////
        /**
         * @brief the setup() function is divided into 4 below,
         * will support config in the future.
         * The order of definitions below is the order when these functions are called
         */
        void setup();
        ///// Sub-procedures of setup ////
        /// PMR pools .etc.
        void setup_general();
        /// Configs
        void setup_config();
        /// Initialize the output target of the logger
        void setup_logger();
        /// Initialize modules,which is handler + thread local data
        void setup_modules();
        /// Initialize route handlers
        void setup_handlers();
        /// Initialize servers (may support workers in the future)
        void setup_servers();

        //// Main Section ////
        /// Launch MNginx
        void run();

        //// Moudle Register Functions ////
        template<HasModuleHandler T,class Policy,class... Policies> inline StateTree::AddResult add_module(const StateNode & tree){
            Policy::template bind<T>(mods);
            return add_module<T,Policies...>(tree);
        }

        template<HasModuleHandler T> inline StateTree::AddResult add_module(const StateNode & tree){
            return handlers.add_new_handler(tree,T::handle);
        }
    };

}

#endif