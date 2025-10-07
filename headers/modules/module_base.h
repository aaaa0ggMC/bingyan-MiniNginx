/**
 * @file module_base.h
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief some essential things for a module
 * @version 0.1
 * @date 2025/10/04
 * 
 * @copyright Copyright(c)2025 aaaa0ggmc
 * 
 * @start-date 2025/10/04 
 */
#ifndef MN_MOD_BASE
#define MN_MOD_BASE
#include <client.h>
#include <epoll.h>
#include <alib-g3/alogger.h>
#include <functional>

namespace mnginx::modules{
    using ModuleInitFn = std::function<void(
        int server_fd,
        EPoll& epoll,
        std::pmr::unordered_map<int,ClientInfo>&,
        alib::g3::LogFactory & lg_acc,
        alib::g3::LogFactory & lg_err
    )>;

    using ModuleTimerFn = std::function<void(double actual_elapse_ms)>;

    template<typename T>
    concept HasModuleInit = requires(T mod,
                                    int server_fd,
                                    EPoll& epoll, 
                                    std::pmr::unordered_map<int, ClientInfo>& clients,
                                    alib::g3::LogFactory& lg_acc,
                                    alib::g3::LogFactory& lg_err) {
        { T::module_init(server_fd, epoll, clients, lg_acc, lg_err) } -> std::same_as<void>;
    };

    template<typename T>
    concept HasModuleTimer = requires(T mod, double elapsed_ms) {
        { T::module_timer(elapsed_ms) } -> std::same_as<void>;
    };

    /// server will copy this struct from application
    struct ModuleFuncs{
        /// before server actaully runs,call these init functions
        std::vector<ModuleInitFn> init;
        /// used to skip existing funcs
        std::vector<int64_t> registered_inits;
        /// timed updates for some modules
        std::vector<ModuleTimerFn> timer;
        /// used to skip existing funcs
        std::vector<int64_t> registered_timers;
    };

    //// Default Policies ////
    /// your module has init function
    struct PolicyInit{
        template<HasModuleInit T> inline static void bind(ModuleFuncs & f){
            // why take address?? 'cause it's compatible with static functions and static functors
            if(std::find(f.registered_inits.begin(),f.registered_inits.end(),(int64_t)&T::module_init)
                == f.registered_inits.end()){
                f.init.push_back(T::module_init);
                f.registered_inits.push_back((int64_t)&T::module_init);
            }
        }
    };

    /// your module has timer function(will be called at a fixed rate)
    struct PolicyTimer{
        template<HasModuleTimer T> inline static void bind(ModuleFuncs & f){
           if(std::find(f.registered_timers.begin(),f.registered_timers.end(),(int64_t)&T::module_timer)
                == f.registered_timers.end()){
                f.timer.push_back(T::module_timer);
                f.registered_timers.push_back((int64_t)&T::module_timer);
            }
        }
    };

    // used to unpack packages
    template<class... Ts> struct policies{}; 
}

// sus defines
/// If you are lazy,use this
#define PolicyFull mnginx::modules::PolicyInit,mnginx::modules::PolicyTimer

#endif