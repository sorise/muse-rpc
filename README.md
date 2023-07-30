# [Muse-RPC](#)
**介绍**: 基于 Connect UDP 和自定义网络协议(简单请求响应协议) 、Reactor 网络模型(one loop per thread + 线程池) 的轻量RPC框架！

- 采用 `C++ 17` 标准内存池
- 方法绑定支持lambda、成员函数、函数符、成员函数指针
- 支持中间件配置
- 目前采用的IO复用方式是同步IO epoll 模型，仅支持 linux(后续会增加 select 以支持windows平台)。



**架构图**：

<img src="./docs/assets/wesrpc.jpg" width="1200px" />



#### 依赖库
- [zlib](https://github.com/madler/zlib)：支持多种压缩算法、应用广泛、是事实上的业界标准的压缩库，需要在机器上提前安装。
- [spdlog](https://github.com/gabime/spdlog):  一个快速、异步、线程安全、高性能的C++ (header-only) 日志库。
- [catch2](https://github.com/catchorg/Catch2) : 一个C++的开源单元测试框架，旨在提供简单、灵活和强大的测试工具。
- [muse-threads](https://github.com/sorise/muse-threads)：基于 C++ 11 标准实现的线程池！
- [muse-serializer](https://github.com/sorise/muse-serializer)：一个实现简单的二进制序列化器。
- [muse-timer](https://github.com/sorise/muse-timer) ：小型的定时器，提供了红黑树定时器和时间轮定时器。



