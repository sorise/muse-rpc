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

int read_str(std::string message, const std::vector<double>& scores){
    return (int)message.size();
}



int main() {
    //注册中间件
    MiddlewareChannel::configure<ZlibService>();  //解压缩
    MiddlewareChannel::configure<RouteService>(Singleton<Registry>(), Singleton<SynchronousRegistry>()); //方法的路由

    /* 设置线程池 */
    ThreadPoolSetting::MinThreadCount = 4;
    ThreadPoolSetting::MaxThreadCount = 4;
    ThreadPoolSetting::TaskQueueLength = 4096;
    ThreadPoolSetting::DynamicThreadVacantMillisecond = 3000ms;

    //启动线程池
    GetThreadPoolSingleton();
    // 启动日志
    InitSystemLogger();

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

    // 开一个线程启动反应堆,等待请求
    Reactor reactor(15000, 2,1500, ReactorRuntimeThread::Asynchronous);

    try {
        //开始运行
        reactor.start();
    }catch (const ReactorException &ex){
        SPDLOG_ERROR("Main-Reactor start failed!");
    }

    std::cin.get();
    reactor.stop();
    spdlog::default_logger()->flush(); //刷新日志
}
