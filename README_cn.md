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
  - [依赖](#依赖)
  - [构建](#构建)
    - [第一步：克隆我的仓库](#第一步克隆我的仓库)
    - [第二步： 运行脚本](#第二步-运行脚本)
  - [开源协议](#开源协议)
  

## 这是什么
一个极简的ngnix服务器。

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


