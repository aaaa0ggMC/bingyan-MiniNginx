#include <modules/reverse_proxy.h>
#include <iostream>

using namespace alib::g3;
using namespace mnginx::modules;
using namespace mnginx;

thread_local ModReverseProxy ModReverseProxy::proxy;

HandleResult ModReverseProxy::handle(HandlerContext ctx){
    /// just send data to server
    LogFactory & lg = *proxy.lg;
    ctx.request.generate();

    return HandleResult::Continue;
}