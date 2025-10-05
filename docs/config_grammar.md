# Config Grammar
## For Main Server
server {
    listen [port];
    server_name [name used to analyse,receive ip address and a unresolved name]

    access_log [where access log are stored]
    error_log [where error log are stored]

    location [PATH] {
        # childrens
        # by default, Host X-Forwarded-For and X-Forwarded-Host are set
        set_header [Header] [value,$host="current host name" is supported];
        set_header [...] [...];

        # a location can have one only,if this field exists,it means that the location's mode is reverse proxy mode 
        proxy_pass [final server path]; 
    }

    location [PATH,eg. /api/v1/{any}] {
        set_header ...

        rule [RULE,eg. /run/media/xxx/{any}]

        index [DEFAULT value when th path is /api/v1 or /api/v1,eg /run/media/xxx/index.html]

        content_type [auto/What ever you set,wrapp with ""]
    }
}

## Some Features
All values can be wrapped with ",so if you want to input ",use \"

## Valid Examples
### 1 [2025/10/05]
```nginx
server {
  # note that comments must end with "\";\"" ;
  # or some sections may not be recognized ; 
  # the name of the server ;
  actually "\"#\"" is not that essential ;
  # but it looks nice ;
  # well you ask me why this system sucks? ;
  # bro,the code of this system is just about 200 lines ;

  server_name localhost;
  
  # port to listen ;
  listen 9191;
  
  # backlog size ;
  backlog 1024;

  # epoll settings ;
  epoll {
    event_list_size 1024;
    wait_interval 10 ms;
    module_least_wait 5 ms;
  };

  location "/api/v1/{path}" {
    type proxy;
   
    target {
      name 127.0.0.1;
      port 7000;
    };
    
  };

  location "api/v2/{path}" {
    type proxy;

    target {
      name 127.0.0.1;
      port 7000;
    };
  };
}
```

