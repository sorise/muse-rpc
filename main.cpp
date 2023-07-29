#include <iostream>
#include <random>
#include <exception>
#include <chrono>
#include <zlib.h>
#include <map>
#include "timer/timer_wheel.hpp"

#include "rpc/protocol/protocol.hpp"
#include "rpc/server/reactor.hpp"
#include "rpc/server/registry.hpp"
#include "rpc/logger/conf.hpp"
#include "rpc/logger/conf.hpp"
#include "rpc/server/reactor.hpp"
#include <functional>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rpc/server/concurrent_registry.hpp"
#include "rpc/server/reqeust.hpp"
#include "thread_pool/pool.hpp"
#include "rpc/server/middleware_channel.hpp"
#include "rpc/server/route_service.hpp"

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
    Normal normal(10, "remix");

    ConcurrentRegistry concurrentRegistry;
    Registry registry;

    concurrentRegistry.Bind("normal", &Normal::addValue, &normal);
    registry.Bind("test_fun1", test_fun1);
    registry.Bind("test_fun2", test_fun2);

    MiddlewareChannel::ConfigureMiddleWare<RouteService>(&registry, &concurrentRegistry);

    GetThreadPoolSingleton(); //启动线程池

    // 启动日志
    InitSystemLogger();
    // 反应堆
    Reactor reactor(15000, 1,1500, ReactorRuntimeThread::Asynchronous);
    try {
        reactor.start();
    }catch (const ReactorException &ex){
        SPDLOG_ERROR("Main-Reactor start failed!");
    }
    std::cin.get();
    spdlog::default_logger()->flush();
}
