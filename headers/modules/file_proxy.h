#ifndef MN_MOD_FP
#define MN_MOD_FP
#include <modules/module_base.h>
#include <alib-g3/alogger.h>
#include <route_states.h>

namespace mnginx{
    using namespace alib::g3;

    constexpr const char * mod_fp_def_root = "/home/aaaa0ggmc/CChaos/";

    struct FileProxyConfig{
        std::pmr::string root;
        std::pmr::string defval;

        inline FileProxyConfig(){
            root = mod_fp_def_root;
            defval = "index.html";
        }
    };

    /// blocked file proxy
    struct ModFileProxy{
        static thread_local ModFileProxy instance;
        
        LogFactory* lg;
        LogFactory* lge;

        /// init module,must be static and use variable "proxy" to access data
        inline static void module_init(int server_fd,
                        EPoll& epoll,
                        std::pmr::unordered_map<int,ClientInfo>& cl,
                        alib::g3::LogFactory & lg_acc,
                        alib::g3::LogFactory & lg_err){
            instance.lg = & lg_acc;
            instance.lge = & lg_err;
            lg_acc(LOG_INFO) << "Module FileProxy Inited" << std::endl;
        }
        

        /// Main function,will be called when route matches
        static HandleResult handle(HandlerContext ctx,const FileProxyConfig & cfg);
    };
} 

#endif
