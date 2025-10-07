<div align="center">

<p align="center">
  <img src="https://github.com/aaaa0ggMC/Blog_PicBackend/blob/main/imgs/bingyan-MiniNginx/logo.png?raw=true" width="60%" alt="MiniNginx" />
</p>

# MiniNginx
这个软件是华中科技大学的[冰岩社团](https://github.com/BingyanStudio)的实习项目。 <br/>
[English](./README.md) / 简体中文

</div>

## 目录
- [MiniNginx](#mininginx)
  - [目录](#目录)
  - [这是什么](#这是什么)
  - [性能](#性能)
    - [配置文件](#配置文件)
    - [内置用于测试的处理器](#内置用于测试的处理器)
    - [无法找到处理器](#无法找到处理器)
    - [对Flask服务器的反向代理](#对flask服务器的反向代理)
      - [Flask QPS](#flask-qps)
      - [高并发 (也许Flask是瓶颈)](#高并发-也许flask是瓶颈)
      - [优化后的并发测试 (并发数目Flask能接受)](#优化后的并发测试-并发数目flask能接受)
      - [静态文件代理](#静态文件代理)
  - [开发博客](#开发博客)
  - [文档](#文档)
  - [依赖](#依赖)
  - [构建](#构建)
    - [第一步：克隆我的仓库](#第一步克隆我的仓库)
    - [第二步： 运行脚本](#第二步-运行脚本)
  - [写在最后](#写在最后)
  - [开源协议](#开源协议)
  

## 这是什么
MiniNginx 是一个高性能、轻量级的 Web 服务器和反向代理服务器，使用纯 C++ 实现。作为冰岩实习项目，它展示了先进的系统编程和网络架构设计能力。
核心功能:
- 🚀 完整的 HTTP 反向代理 - 支持完整的 HTTP/1.1 协议和请求转发
- 📁 静态文件服务器 - 高效的静态资源服务，支持目录索引
- ⚡ Epoll 驱动架构 - 高并发 I/O 多路复用，支持 10,000+ 并发连接
- 🔄 Master-Worker 进程模型 - 多进程架构，优化资源利用
- 🛠 零第三方网络库依赖 - 完全自主实现的 HTTP 协议栈
- 📊 完整的日志系统 - 访问日志和错误日志，支持文件轮转和锁保护
- ⚙️ 热重载配置 - 动态更新配置无需重启服务器
- 🧩 模块化设计 - 可扩展的模块系统，易于功能扩展
- 🛡 内存安全 - PMR 内存池实现，防止内存溢出
- 🔄 HTTP 方法支持 - GET、POST、PUT、DELETE、PATCH、HEAD、OPTIONS、TRACE、CONNECT
- 📦 分块传输编码 - 支持 HTTP 分块数据传输
- 🔗 连接保活 - 心跳机制维持持久连接
- 🎯 URL 路由 - 灵活的路由匹配和路径解析

## 性能
### 配置文件
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
### 内置用于测试的处理器
```bash
# 预期输出： <h1>HelloWorld</h1>nihao
wrk -t24 -c1000 -d8s http://localhost:9191/handler/nihao 
```

| 属性 | 数值 |
| :-------: | :-------: |
| QPS | 11,589.64 |
| 平均延迟 | 83.06ms |
| 最大延迟 | 192.68ms |
| 请求总数 | 93,722 |
| 流量 | 1.89MB/s |
| 并发连接数 | 1000 |
| 测试时长 | 8.09s |

### 无法找到处理器
路径状态机无法找到对于路径的处理器，返回404。
```bash
wrk -t24 -c1000 -d8s http://localhost:9191/handlers/nihao  # /handlers/{any} 不存在！
```
| 属性 | 数值 |
| :-------: | :-------: |
| QPS | 32,174.03 |
| 平均延迟 | 30.49ms |
| 最大延迟 | 99.36ms |
| 请求总数 | 260,587 |
| 流量 | 1.41MB/s |
| 并发数目 | 1,000 |
| 测试时长 | 8.10s |
| 404返回率 | 100% |

### 对Flask服务器的反向代理
性能分析: 由于Flask为单线程服务器而mngix目前未对单线程服务器的转发进行过多的优化导致——当并发数目是Flask能接受的数目的时候转发损失为10%,而当并发数目远大于Flask能接受的程度时转发损失高达97%，存在大量timeout。

#### Flask QPS
2800-2900

#### 高并发 (也许Flask是瓶颈)
```bash
wrk -t1 -c1000 -d8s http://localhost:9191/api/v1/
```
| 属性 | 数值 |
| :-------: | :-------: |
| QPS | 83.72 |
| 平均延迟 | 1.07s |
| 最大延迟 | 1.08s |
| 请求数目 | 677 |
| 并发数目 | 1,000 |
| 测试时长 | 8.09s |
| 超时率 | 79.5% |

#### 优化后的并发测试 (并发数目Flask能接受)
```bash
# 32 connections matches Flask's optimal capacity
wrk -t1 -c32 -d8s http://localhost:9191/api/v1/
```
| 属性 | 数值 |
| :-------: | :-------: |
| QPS | 2,559.75 |
| 平均延迟 | 11.99ms |
| 最大延迟 | 55.04ms |
| 请求总数 | 20,490 |
| 流量 | 479.95KB/s |
| 测试时长 | 8.10s |
| 错误率 | 0% |

#### 静态文件代理
由于目前还没做文件缓存，因此测试环境为24个线程24，000个连接对着同一个文件不断读取。
```bash
wrk -t24 -c1000 -d8s http://localhost:9191/file/lets_chat.c
```
| 属性 | 数值 |
| :-------: | :-------: |
| QPS | 3,172.12 |
| 平均延迟 | 40.59ms |
| 请求总数 | 25,689 |
| 流量 | 6.77MB/s |
| 读取错误 | 48 |
| 返回404/403 | 24 |

## 开发博客
[DevBlog:Mininginx = 1](https://aaaa0ggmc.github.io/Blog/keep_learning/c_cpp/devlog_mininginx.html)

## 文档
[Mingnix文档](https://aaaa0ggmc.github.io/bingyan-MiniNginx/MiniNginx/)

## 依赖
- C++ (GCC 或者 Clang 或者随便)
  - 编译器必须支持 -std=c++26 选项
- Google Test

## 构建
### 第一步：克隆我的仓库
执行 
```bash
git clone https://github.com/aaaa0ggMC/bingyan-MiniNginx.git
cd bingyan-MiniNginx
git checkout main
```

### 第二步： 运行脚本
执行
```bash
bash ./scripts/configure
bash ./scripts/build
```
生成的可执行文件在 ./CBuild/这个文件夹里面

## 写在最后
挺好的，与其说是实习项目不如说是一次有目标的锻炼，成功让我在7天内从仅仅了解TCP/UDP是什么的人大致了解了更加复杂的网络处理，也算是让我网络方面的水平进了一步了。同时这也让我能更加熟悉自己开发的节奏，懂得什么时候进行codereview以及什么时候摸鱼啥的，更重要的是这次项目还让我对于软件架构的处理更加娴熟了，就比如目前我的MiniNginx就是完全可拓展的形态，无论是配置文件还是Module，我的热插拔设计也让我开发多线程和配置热加载十分顺利。不过我累了（其实是近几天看王者解说视频有点小上瘾），所以留了个负载均衡没细做，不过难度应该不会太大，简易版本的话直接均分给负载服务器，复杂一点可能就要复杂服务器配合或者是定期发送心跳包查看延迟来判断啥的，反正也不简单。 <br/>
Anyway, <br/>
<p align='right'>
Cheers! <br/>
2025/10/07
</p>

## 开源协议
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

