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
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89
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


int main() {
    //绑定方法的例子
    Normal normal(10, "remix");

    muse_bind_sync("normal", &Normal::addValue, &normal);
    muse_bind_async("test_fun1", test_fun1);
    muse_bind_async("test_fun2", test_fun2);

    //注册中间件
    MiddlewareChannel::configure<ZlibService>();  //解压缩
    MiddlewareChannel::configure<RouteService>(Singleton<Registry>(), Singleton<SynchronousRegistry>()); //方法的路由

    //启动线程池
    GetThreadPoolSingleton();
    // 启动日志
    InitSystemLogger();
    // 开一个线程启动反应堆,等待请求
    Reactor reactor(15000, 2,1500, ReactorRuntimeThread::Asynchronous);

    try {
        //开始运行
        reactor.start();
    }catch (const ReactorException &ex){
        SPDLOG_ERROR("Main-Reactor start failed!");
    }

    std::cin.get();
    spdlog::default_logger()->flush(); //刷新日志
}
