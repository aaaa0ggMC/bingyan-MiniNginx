<div align="center">

<p align="center">
  <img src="https://github.com/aaaa0ggMC/Blog_PicBackend/blob/main/imgs/bingyan-MiniNginx/logo.png?raw=true" width="60%" alt="MiniNginx" />
</p>

# MiniNginx
This is a software developed as the internship project for [Bingyan Studio](https://github.com/BingyanStudio) of HUST(Huazhong University of Science and Technology). <br/>
English / [简体中文](./README_cn.md)

</div>

## Table of Contents
- [MiniNginx](#mininginx)
  - [Table of Contents](#table-of-contents)
  - [What is it](#what-is-it)
  - [Performance](#performance)
    - [Config](#config)
    - [Internal TestHandler](#internal-testhandler)
    - [Missed Handler](#missed-handler)
    - [Reverse Proxy to Flask Server](#reverse-proxy-to-flask-server)
      - [Flask QPS](#flask-qps)
      - [High Concurrency (Flask Bottleneck)](#high-concurrency-flask-bottleneck)
      - [Optimal Concurrency Test (Matching Flask Capacity)](#optimal-concurrency-test-matching-flask-capacity)
      - [File Proxy](#file-proxy)
  - [Dependencies](#dependencies)
  - [DevBlog](#devblog)
  - [Docs](#docs)
  - [Building](#building)
    - [Step1: clone this repo](#step1-clone-this-repo)
    - [Step2: run scripts](#step2-run-scripts)
  - [Final Thoughts (Translated from 写在最后 by DeepSeek)](#final-thoughts-translated-from-写在最后-by-deepseek)
    - [🚀 Technical Growth](#-technical-growth)
    - [🏗 Engineering Skills Enhancement](#-engineering-skills-enhancement)
    - [🎯 Project Achievements](#-project-achievements)
  - [License](#license)
  

## What is it
MiniNginx is a high-performance, lightweight web server and reverse proxy server implemented in pure C++. It serves as the internship project for Bingyan Studio, demonstrating advanced system programming and network architecture design.
Key Features:
- 🚀 Complete HTTP Reverse Proxy - Full HTTP/1.1 protocol support with request forwarding
- 📁 Static File Server - Efficient static resource serving with directory indexing
- ⚡ Epoll-driven Architecture - High-concurrency I/O multiplexing supporting 10,000+ concurrent connections
- 🔄 Master-Worker Process Model - Multi-process architecture for better resource utilization
- 🛠 Zero Third-party Network Library Dependencies - Entirely self-implemented HTTP protocol stack
- 📊 Comprehensive Logging System - Access & error logs with file rotation and locking
- ⚙️ Hot Reload Configuration - Dynamic configuration updates without server restart
- 🧩 Modular Design - Extensible module system for easy functionality expansion
- 🛡 Memory Safety - PMR memory pool implementation preventing OOM issues
- 🔄 HTTP Methods Support - GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, TRACE, CONNECT
- 📦 Chunked Transfer Encoding - Support for HTTP chunked data transmission
- 🔗 Connection Keep-alive - Heartbeat mechanism for persistent connections
- 🎯 URL Routing - Flexible request routing with path matching

## Performance
### Config
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

  # worker counts ;
  workers 8;

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

  location "file/{path}" {
    type file;
  
    root "/home/aaaa0ggmc/CChaos";
    index index.html;
  };
}
```
### Internal TestHandler
```bash
# which is expected to receive <h1>HelloWorld</h1>nihao
wrk -t24 -c1000 -d8s http://localhost:9191/handler/nihao 
```

| Property | Value |
| :-------: | :-------: |
| QPS | 11,589.64 |
| Avg.Latency | 83.06ms |
| Max.Latency | 192.68ms |
| Total Requests | 93,722 |
| Throughput | 1.89MB/s |
| Concurrent Connections | 1000 |
| Test Duration | 8.09s |

### Missed Handler
The route tree cant found related handler to the path,mnginx returns 404 immediately.
```bash
wrk -t24 -c1000 -d8s http://localhost:9191/handlers/nihao  # /handlers/{any} doesnt exist! 
```
| Property | Value |
| :-------: | :-------: |
| QPS | 32,174.03 |
| Avg.Latency | 30.49ms |
| Max.Latency | 99.36ms |
| Total Requests | 260,587 |
| Throughput | 1.41MB/s |
| Concurrent Connections | 1,000 |
| Test Duration | 8.10s |
| Error Rate | 100% |

### Reverse Proxy to Flask Server
Performance Analysis: The reverse proxy shows excellent performance when concurrency matches Flask's capacity, but bottlenecks occur with excessive concurrency due to Flask's single-threaded nature.
#### Flask QPS
2800-2900

#### High Concurrency (Flask Bottleneck)
```bash
# Flask development server is single-threaded
wrk -t1 -c1000 -d8s http://localhost:9191/api/v1/
```
| Property | Value |
| :-------: | :-------: |
| QPS | 83.72 |
| Avg.Latency | 1.07s |
| Max.Latency | 1.08s |
| Total Requests | 677 |
| Concurrent Connections | 1,000 |
| Test Duration | 8.09s |
| Timeout Rate | 79.5% |

#### Optimal Concurrency Test (Matching Flask Capacity)
```bash
# 32 connections matches Flask's optimal capacity
wrk -t1 -c32 -d8s http://localhost:9191/api/v1/
```
| Property | Value |
| :-------: | :-------: |
| QPS | 2,559.75 |
| Avg.Latency | 11.99ms |
| Max.Latency | 55.04ms |
| Total Requests | 20,490 |
| Throughput | 479.95KB/s |
| Test Duration | 8.10s |
| Error Rate | 0% |

#### File Proxy
No file caching support currently,which means that 24 threads(24,000 connections) are competing for the fd of that file. 
```bash
wrk -t24 -c1000 -d8s http://localhost:9191/file/lets_chat.c
```
| Property | Value |
| :-------: | :-------: |
| QPS | 3,172.12 |
| Avg.Latency | 40.59ms |
| Total Requests | 25,689 |
| Throughput | 6.77MB/s |
| Read Errors | 48 |
| Error Responses | 24 |

## Dependencies
- C++ (GCC or Clang or what ever)
  - The compiler you choose must support -std=c++26
- Google Test

## DevBlog
[DevBlog:Mininginx = 1](https://aaaa0ggmc.github.io/Blog/keep_learning/c_cpp/devlog_mininginx.html)

## Docs
see [MiniNgnix doc](https://aaaa0ggmc.github.io/bingyan-MiniNginx/MiniNginx/)

## Building
### Step1: clone this repo
run 
```bash
git clone https://github.com/aaaa0ggMC/bingyan-MiniNginx.git
cd bingyan-MiniNginx
git checkout main
```

### Step2: run scripts
run
```bash
bash ./scripts/configure
bash ./scripts/build
```
build results are stored in ./CBuild/

## Final Thoughts (Translated from [写在最后](./README_cn.md#写在最后) by DeepSeek)
This project has been more than just an internship assignment—it was a targeted technical deep dive that yielded significant growth.
### 🚀 Technical Growth
- Progressed from basic TCP/UDP knowledge to mastering complete network programming architecture
- Gained deep understanding of core concepts including Epoll, multi-process models, and HTTP protocol
- Achieved substantial advancement in network programming capabilities
### 🏗 Engineering Skills Enhancement
- Mastered modular, extensible software architecture design
- Implemented hot-pluggable designs that streamlined multi-threading and hot-reload development
- Improved proficiency in code review and development rhythm management
### 🎯 Project Achievements
The current MiniNginx boasts comprehensive production-ready features with an architecture designed for future extensibility. While the load balancing functionality awaits further implementation due to time constraints, the foundational framework is firmly in place.
**In summary, this has been an immensely rewarding technical journey!**
<p align='right'>
Cheers!<br/>
2025/10/07
</p>

## License
<pre>
MIT License

Copyright (c) 2025 RockonDreaming

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
</pre>


