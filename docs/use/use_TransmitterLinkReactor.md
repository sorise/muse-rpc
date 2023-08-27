### [TransmitterLinkReactor 类](#)
**介绍**：将我们的Reactor 附加一个主动发送RPC请求的能力，这个请求将会从服务端的端口发出。

----
这是一个用于让RPC服务端主反应堆可以主动向某个其他服务器主动发起请求，其实我们也可以在服务端调用
Transmitter或者Client对象创建一个客户端然后调用远程方法,**但是这种方法通常从操作系统获取一个随机的端口号发送RPC请求**。

如果你的分布式系统中的所有服务器都具有公网IP，或者位于统一的一个局域网情况下，当然可以启动一个客户端来发起请求。
但如果你的一些服务器不具备公网IP，同时又不想使用内网穿透工具，那么可以采用主动式 Reactor，里面会创建一个
TransmitterLinkReactor对象，可以实现这个功能！

**创建一个主动式Reactor** : 创建方法非常的简单，因为只需要传递一个 bool 值，`reactor.start(true)`。

```cpp
/*
 * // 注册中间件
 * // 设置线程池 
 * // 启动线程池
 * // 启动日志
 * */
// 开一个线程启动反应堆,等待请求
Reactor reactor(15000, 2,2000, ReactorRuntimeThread::Asynchronous);

try {
    //开始运行,创建一个主动式的reactor
    reactor.start(true);
}catch (const ReactorException &ex){
    SPDLOG_ERROR("Main-Reactor start failed!");
}

//等待发送器启动完毕
reactor.wait_transmitter();

TransmitterEvent event("125.91.127.142", 1450);
event.call<int>("test_fun1", i);
event.set_callBack(deal_handler); //设置回调函数

//让服务端发送RPC请求，数据将会从端口 15000 发出
reactor.send(std::move(event));
```