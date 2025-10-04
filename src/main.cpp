#include <application.h>
#include <gtest/gtest.h>

int main(int argc,char ** argv){
#ifdef MNGINX_TESTING
    testing::InitGoogleTest(&argc,argv);
    return RUN_ALL_TESTS();
#endif
    mnginx::Application app;
    app.setup();
    app.run();
    
    return app.return_result;
}