#include <application.h>
#include <gtest/gtest.h>

int main(){
#ifdef MNGINX_TESTING
    testing::InitGoogleTest();
    return RUN_ALL_TESTS();
#endif

    mnginx::Application app;
    app.setup();
    app.run();

    return app.return_result;
}