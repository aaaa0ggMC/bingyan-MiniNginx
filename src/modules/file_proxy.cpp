#include <modules/file_proxy.h>
#include <alib-g3/autil.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <socket_util.h>

using namespace mnginx;

thread_local ModFileProxy ModFileProxy::instance;

HandleResult ModFileProxy::handle(HandlerContext ctx,const FileProxyConfig & cfg){
    static std::string file_path;\
    file_path.clear();
    file_path = cfg.root;
    {
        auto str = ctx.get_param("path");
        if(!str){
            /// build a 404
            ctx.response.status_code = HTTPResponse::StatusCode::NotFound;
            ctx.response.status_str = "Not Found";
            return HandleResult::Close;
        }else if(!str->compare("..") || str->find("../") != std::string_view::npos){
            ctx.response.status_code = HTTPResponse::StatusCode::Forbidden;
            ctx.response.status_str = "Forbid ../";
            return HandleResult::Close;
        }
        if(file_path.empty() || *(file_path.end()-1) != '/'){
            file_path.push_back('/');
        }
        if((*str).empty())file_path += cfg.defval;
        else if(*(str->begin()) == '/')file_path += std::string_view(str->begin()+1,str->end());
        else file_path += *str;
    }
    long file_size = alib::g3::Util::io_fileSize(file_path);
    (*instance.lg)(LOG_INFO) << file_path << endlog;
    
    if(file_size < 0){
        ctx.response.status_code = HTTPResponse::StatusCode::NotFound;
        ctx.response.status_str = "Not Found";
        return HandleResult::Continue;
    }
    // headers
    ctx.response.headers[KEY_Content_Length] = std::to_string(file_size);
    ctx.response.headers["Content-Type"] = "text/plain; charset=utf-8";

    auto data = ctx.response.generate();
    if(send(ctx.fd,data.data(),data.size(),MSG_NOSIGNAL) < 0){
        return HandleResult::Close; // bad connection
    }
    // send extra files
    int file_fd = open(file_path.c_str(),O_RDONLY);
    off_t offset = 0;
    long remaining = file_size;
    fcntl(ctx.fd,F_SETFL,fcntl(ctx.fd,F_GETFL,0) & ~O_NONBLOCK);

    defer{
        fcntl(ctx.fd,F_SETFL,fcntl(ctx.fd,F_GETFL,0) & ~O_NONBLOCK);
        close(file_fd);
    };

    while (remaining > 0) {
        ssize_t s = sendfile(ctx.fd,file_fd,&offset,remaining);
        if(s < 0){
            if(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN){
                usleep(1); // skip for some cycles
                continue;
            }else{
                (*instance.lge)(LOG_INFO) << "failed to send file to fd[" << ctx.fd << "]" << endlog;
                return HandleResult::Close;
            }
        }else remaining -= s;
    }
    return HandleResult::AlreadySend;
}