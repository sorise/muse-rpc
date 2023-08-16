#include <iostream>
#include <random>
#include <exception>
#include <chrono>
#include <zlib.h>
#include <map>
#include "timer/timer_wheel.hpp"
#include "rpc/rpc.hpp"
#include <functional>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "thread_pool/pool.hpp"

using namespace muse::rpc;
using namespace muse::pool;
using namespace std::chrono_literals;

class Normal{
public:
    Normal(int _value, std::string  _name)
            :value(_value), name(std::move( _name)){}

    std::string setValueAndGetName(int _new_value){
        this->value = _new_value;
        return this->name;
    }

    int getValue() const{
        return this->value;
    }

    void addValue(){
        printf("\n add 1  \n");
        this->value++;
    }

private:
    int value;
    std::string name;
};

int test_fun1(int value){
    int i  = 10;
    return i + value;
}

std::vector<double> test_fun2(const std::vector<double>& scores){
    std::vector<double> tr = {
          89,78,58,94,15,65,94,100,12.52
    };
    return tr;
}

class zlibService: public middleware_service{
public:
    zlibService() = default;
    ~zlibService() override = default;

    //数据输入 解压、解密
    std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
    In(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override{
        printf("jie ya\n");
        return std::make_tuple(data, data_size, _memory_pool);
    };

    //数据输出 压缩、加密
    std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
    Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override{
        printf("ya suo\n");
        return std::make_tuple(data, data_size, _memory_pool);
    };
};

int read_str(std::string message, const std::vector<double>& scores){
    return (int)message.size();
}

int main() {
//    Peer peer {14500,2130706433};
//    GlobalEntry::con_queue->insert_or_assign(peer, SocketConnection { 1,1 });
//
//    Peer peer1 {14500,2130706433};
//    auto it = GlobalEntry::con_queue->find(peer1);
//    if (it == GlobalEntry::con_queue->end()){
//        printf("what?");
//    }else{
//        printf("what????");
//    }


    //绑定方法的例子
    Normal normal(10, "remix");

    muse_bind_sync("normal", &Normal::addValue, &normal);
    muse_bind_async("test_fun1", test_fun1);
    muse_bind_async("test_fun2", test_fun2);
    muse_bind_async("read_str", read_str);

    muse_bind_async("lambda test", [](int val)->int{
        printf("why call me \n");
        return 10 + val;
    });

    //注册中间件
    MiddlewareChannel::configure<ZlibService>();  //解压缩
    MiddlewareChannel::configure<RouteService>(Singleton<Registry>(), Singleton<SynchronousRegistry>()); //方法的路由

    //启动线程池
    GetThreadPoolSingleton();
    // 启动日志
    InitSystemLogger();
    // 开一个线程启动反应堆,等待请求
    Reactor reactor(15000, 1,1500, ReactorRuntimeThread::Asynchronous);

    try {
        //开始运行
        reactor.start();
    }catch (const ReactorException &ex){
        SPDLOG_ERROR("Main-Reactor start failed!");
    }

    std::cin.get();
    GetThreadPoolSingleton()->close(); //关闭默认线程池
    spdlog::default_logger()->flush(); //刷新日志
}
