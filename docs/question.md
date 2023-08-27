#### 问题 1 主从反应堆交互与内部数据结构
主从反应堆模型中，从反应堆如何什么数据维持链接、同时如何存储完整的报文数据，socket如何存储和将达到超时限制的socket删除出去

其次：由于主反应堆会发送数据到从反应堆，对数据由许多处理，应该以什么流程取处理，反应堆应该以什么逻辑运行以降低加锁的次数和粒度。


**解决方法：** 利用一个缓存队列，等到epoll timeout 由从反应堆线程自己处理。

#### 问题 2 要不要建立一个发送缓存区！
当然需要，那么发送缓存区由什么对象来管理，怎么使用？？？客户端应该是工厂模式，还是家庭作坊模式？？？

#### 问题 3 一个线程一个内存池?
还是一个程序一个内存池

#### 问题 4 函数指针能赋值给万能引用？
为啥不能自动类型推断
```c++
template<typename F>
void Bind(const std::string& name, F&& func){
    dictionary[name] = std::bind(&Registry::Proxy<F>, 
            this, std::forward<F>(func), std::placeholders::_1);
}

int test_fun1(int value){
    int i  = 10;
    return i + value;
}

int main() {
    Bind("run", test_fun1); //失败 ，是不行的
}
```

#### 问题 5 如何保证成员函数调用的线程安全性,数据竞争问题
[SpringMVC：如何保证Controller的并发安全？](https://blog.csdn.net/u012811805/article/details/130787882)
 
* 尽量不要在 Controller 中定义成员变量；
* 如果必须要定义一个非静态成员变量，那么可以通过注解 @Scope(“prototype”) 将Controller设置为多例模式。
* Controller 中使用 ThreadLocal 变量。每一个线程都有一个变量的副本。
* Spring本身并没有解决并发访问的问题。

#### 问题 6 定时器导致死锁

#### 问题 7 tcache_thread_shutdown(): unaligned tcache chunk detected

#### 问题 8 socket是线程安全的吗
多线程并发读/写同一个TCP socket 是 线程安全 的，因为TCP socket 的读/写操作都上锁了。

#### 问题 9 如何传递客户端请求 IP/Port 给方法