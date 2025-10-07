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
  - [文档](#文档)
  - [依赖](#依赖)
  - [构建](#构建)
    - [第一步：克隆我的仓库](#第一步克隆我的仓库)
    - [第二步： 运行脚本](#第二步-运行脚本)
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


