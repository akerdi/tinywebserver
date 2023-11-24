# TinyWebServer

这是一篇针对[qinguoyi/TinyWebServer](https://github.com/qinguoyi/TinyWebServer) 的学习文章。

TinyWebServer 以基于socket + epoll 等基础api构建最基础的服务为目标。最终实现如使用Nodejs的http 构建监听端口并接收接口信息，发送页面。其中实现方式都是最简化方案。以商用、生产环境为目的上线，则需要优化对应的模块实现。

如果要快速实现，推荐使用libuv，libevent等知名库，本文仅方便了解Web服务实现，了解并跟着实现其中最基础实现方式。在学习之前，提前先了解下相关api知识，请查看[博主网页](https://zwiley.github.io/mybook/unix_linux/Linux%E9%AB%98%E6%80%A7%E8%83%BD%E6%9C%8D%E5%8A%A1%E5%99%A8%E7%BC%96%E7%A8%8B/Linux%E9%AB%98%E6%80%A7%E8%83%BD%E6%9C%8D%E5%8A%A1%E5%99%A8%E7%BC%96%E7%A8%8B/)罗列的相关api。该博主还有与TinyWebServer 相关的博文，有兴趣可以查看。本篇文章和该博文区别在于更注重写和实现。

## Environment

```
Platform: Windows
IDE     : VSCode
Running : WSL-ubuntu
```

## Content

以下按章节循序渐进学习:

+ [x] sem / lock / cond
+ [x] block_queue
+ [x] log
+ [x] sql pool && RAII interface
+ [x] lst_timer && utils
+ [x] threadpool
+ [x] webserver(constitude all logic above)
+ [ ] http logic

## Ref

https://beej-zhcn.netdpi.net/

https://github.com/akerdi/sigAttachEpoll_sample

https://github.com/qinguoyi/TinyWebServer

https://zwiley.github.io/mybook/
