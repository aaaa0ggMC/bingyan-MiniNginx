enum class StatusCode : int32_t {
    // 2xx Success
    OK = 200,                        // 请求成功
    Created = 201,                   // 资源创建成功
    Accepted = 202,                  // 请求已接受，处理中
    NoContent = 204,                 // 请求成功，无返回内容

    // 3xx Redirection  
    MovedPermanently = 301,          // 资源永久重定向
    Found = 302,                     // 资源临时重定向
    NotModified = 304,               // 资源未修改，使用缓存

    // 4xx Client Errors
    BadRequest = 400,                // 请求语法错误
    Unauthorized = 401,              // 需要身份验证
    Forbidden = 403,                 // 服务器拒绝请求
    NotFound = 404,                  // 资源未找到
    MethodNotAllowed = 405,          // 请求方法不允许
    RequestTimeout = 408,            // 请求超时
    Conflict = 409,                  // 请求冲突
    PayloadTooLarge = 413,           // 请求体过大
    URITooLong = 414,                // URI过长
    UnsupportedMediaType = 415,      // 不支持的媒体类型
    TooManyRequests = 429,           // 请求过于频繁

    // 5xx Server Errors
    InternalServerError = 500,       // 服务器内部错误
    NotImplemented = 501,            // 功能未实现
    BadGateway = 502,                // 网关错误
    ServiceUnavailable = 503,        // 服务不可用
    GatewayTimeout = 504,            // 网关超时
    HTTPVersionNotSupported = 505    // HTTP版本不支持
};

// 辅助函数：获取状态码对应的标准描述
constexpr static std::string_view get_status_description(StatusCode code) {
    switch (code) {
        case StatusCode::OK: return "OK";
        case StatusCode::Created: return "Created";
        case StatusCode::Accepted: return "Accepted"; 
        case StatusCode::NoContent: return "No Content";
        case StatusCode::MovedPermanently: return "Moved Permanently";
        case StatusCode::Found: return "Found";
        case StatusCode::NotModified: return "Not Modified";
        case StatusCode::BadRequest: return "Bad Request";
        case StatusCode::Unauthorized: return "Unauthorized";
        case StatusCode::Forbidden: return "Forbidden";
        case StatusCode::NotFound: return "Not Found";
        case StatusCode::MethodNotAllowed: return "Method Not Allowed";
        case StatusCode::RequestTimeout: return "Request Timeout";
        case StatusCode::Conflict: return "Conflict";
        case StatusCode::PayloadTooLarge: return "Payload Too Large";
        case StatusCode::URITooLong: return "URI Too Long";
        case StatusCode::UnsupportedMediaType: return "Unsupported Media Type";
        case StatusCode::TooManyRequests: return "Too Many Requests";
        case StatusCode::InternalServerError: return "Internal Server Error";
        case StatusCode::NotImplemented: return "Not Implemented";
        case StatusCode::BadGateway: return "Bad Gateway";
        case StatusCode::ServiceUnavailable: return "Service Unavailable";
        case StatusCode::GatewayTimeout: return "Gateway Timeout";
        case StatusCode::HTTPVersionNotSupported: return "HTTP Version Not Supported";
        default: return "Unknown Status";
    }
}