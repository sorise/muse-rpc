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
    Reactor reactor(15000, 2,2000, ReactorRuntimeThread::Asynchronous);

    try {
        //开始运行
        reactor.start(true);
    }catch (const ReactorException &ex){
        SPDLOG_ERROR("Main-Reactor start failed!");
    }

    //等待发送器启动完毕
    reactor.wait_transmitter();

    for (int i = 0; i < 5; ++i) {
        TransmitterEvent event("125.91.127.142", 15000);
        event.call<int>("test_fun1", i);
        event.set_callBack([](Outcome<int> t){
            if (t.isOK()){
                printf("OK lambda %d \n", t.value);
            }else{
                //调用失败
                if (t.protocolReason == FailureReason::OK){
                    //错误原因是RPC错误
                    std::printf("rpc error\n");
                    std::cout << t.response.getReason() << std::endl;
                    //返回 int 值对应 枚举 RpcFailureReason
                }else{
                    //错误原因是网络通信过程中的错误
                    std::printf("internet error\n");
                    std::cout << (short)t.protocolReason << std::endl; //错误原因
                }
            }
        });
        reactor.send(std::move(event));
    }

    std::cin.get();
    spdlog::default_logger()->flush(); //刷新日志
}
