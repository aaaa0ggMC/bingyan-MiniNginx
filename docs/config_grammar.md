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