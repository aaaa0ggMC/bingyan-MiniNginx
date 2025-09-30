#include <alib-g3/alogger.h>
#include <http_parser.h>
#include <memory_resource>

// Since my library is verbose,so here I globally use the namespace
using namespace alib::g3;

namespace mnginx{
    class Application{
    private:
        //// Log System
        Logger logger;
        LogFactory lg;
        
        //// Memory Management
        std::pmr::synchronized_pool_resource pool;
        std::pmr::memory_resource * resource;


    public:
        int return_result;

        Application();

        //// Setup Section ////
        void setup();
        void setup_general();
        void setup_logger();

        //// Main Section ////
        void run();
    };

}