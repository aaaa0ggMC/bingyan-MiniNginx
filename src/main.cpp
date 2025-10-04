#include <application.h>
#include <gtest/gtest.h>
#include <csignal>
#include <cstdlib>

// to deconstruct when std::exit called
mnginx::Application app;

int main(int argc,char ** argv){
#ifdef MNGINX_TESTING
    testing::InitGoogleTest(&argc,argv);
    return RUN_ALL_TESTS();
#endif
    std::signal(SIGINT,[](int){
        std::exit(0);
    });

    app.setup();
    app.run();
    
    return app.return_result;
}