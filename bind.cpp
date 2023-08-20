#include <iostream>
#include <random>
#include <exception>
#include <chrono>
#include <zlib.h>
#include "rpc/protocol/protocol.hpp"
#include "timer/timer_wheel.hpp"
#include "rpc/server/reactor.hpp"
#include "rpc/logger/conf.hpp"
#include "rpc/server/reactor.hpp"
#include "rpc/protocol/protocol.hpp"
#include "serializer/binarySerializer.h"
#include "rpc/client/invoker.hpp"
#include "rpc/client/client.hpp"
#include "timer/timer_wheel.hpp"
#include "rpc/client/response_data.hpp"
#include "rpc/client/transmitter.hpp"
#include "rpc/memory/conf.hpp"
#include "rpc/client/global_id.hpp"
#include "rpc/rpc.hpp"

using namespace muse::rpc;
using namespace muse::timer;
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

    void func(Outcome<int> outcome){
        if (outcome.isOK()){
            std::cout << outcome.value << std::endl;
        }
    }
private:
    int value;
    std::string name;
};


void test(){
    //注册中间件
    MiddlewareChannel::configure<ZlibService>();  //解压缩
    MiddlewareChannel::configure<RouteService>(
            Singleton<Registry>(), Singleton<SynchronousRegistry>()
    ); //方法的路由
    // 125.91.127.142    // 127.0.0.1
    Client remix("125.91.127.142 ", 14000, MemoryPoolSingleton());

    std::vector<double> scores = {
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
    };

    auto result =remix.call<std::vector<double>>("test_fun2",scores);
    if (result.isOK()){
        std::printf("success\n");
        std::cout << result.value.size() << std::endl;
    }else{
        std::printf("failed\n");
    }

    auto ri = remix.call<int>("test_fun1", 590);
    if (ri.isOK()){
        std::cout << ri.value << std::endl;
    }else{
        if (ri.protocolReason == FailureReason::OK){
            std::printf("rpc error\n");
        }else{
            std::printf("protocol error\n");
        }
    }
}

void func(Outcome<int> outcome){
    if (outcome.isOK()){
        std::cout << outcome.value << std::endl;
    }else{
        std::cout << "what !" <<std::endl;
    }
}

void what(std::function<void(Outcome<int>)> f){
    muse::rpc::Outcome<int> t;
    f.operator()(t);
}

void testfunc(Outcome<int> t){
    if (t.isOK()){
        printf("OK lambda %d \n", t.value);
    }else{
        printf("fail lambda\n");
    }
}

void test_v(){
    //注册中间件
    MiddlewareChannel::configure<ZlibService>();  //解压缩
    MiddlewareChannel::configure<RouteService>(
            Singleton<Registry>(), Singleton<SynchronousRegistry>()
    ); //方法的路由

    Transmitter transmitter(14555, GetThreadPoolSingleton());

    std::vector<double> score = {
            100.526,95.84,75.86,99.515,6315.484,944.5,98.2,99898.26,9645.54,484.1456,8974.4654,4894.156,
            89,12,0.56,95.56,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,115.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.28,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,155.89,84.01,98.1,15.2,98.2,15.89,
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
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,84.01,98.1,15.2,98.2,15.89,
            78,84,59,68,49,75
    };

    std::string name {
            "asdasd54986198456h487s1as8d7as5d1w877y98j34512g98adsf3488as31c98a sd3871198The built-in string class provides the ability to do complex variable substitutions and value formatting via the format() method described in PEP 3101. The Formatter class in the string module allows you to create and customize your own string formatting behaviors using the same implementation as the built-in format() method. This function does the actual work of formatting. It is exposed as a separate function for cases where you want to pass in a predefined dictionary of arguments, rather than unpacking and repacking the dictionary as individual arguments using the *args and **kwargs syntax. vformat() does the work of breaking up the format string into character data and replacement fields. It calls the various methods described below."
    };

    for (int i = 0; i < 1000; ++i) {
        TransmitterEvent event("125.91.127.142", 15000);
        event.call<int>("read_str", name,score);
        event.set_callBack([](Outcome<int> t){
            if (t.isOK()){
                printf("OK lambda %d \n", t.value);
            }else{
                printf("fail lambda\n");
            }
        });
        transmitter.send(std::move(event));
    }

    transmitter.start(TransmitterThreadType::Asynchronous);

    transmitter.stop();
}

int main(int argc, char *argv[]){
    int epollFd = -1;
    //创建 epoll
    if ((epollFd = epoll_create(128)) == -1){
        SPDLOG_ERROR("Sub-Reactor loop function failed epoll fd created failed!");
    }
    struct epoll_event epollQueue[50];

    int socket_virtual_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct epoll_event ReactorEpollEvent{ EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP, {.fd = socket_virtual_fd} };

    if( epoll_ctl(epollFd, EPOLL_CTL_ADD, socket_virtual_fd, &ReactorEpollEvent) == - 1 ){
        close(socket_virtual_fd); //关闭 socket
        SPDLOG_ERROR("Epoll Operation EPOLL_CTL_ADD failed in Sub-Reactor deal with new Connection errno {} !", errno);
    }else{
        SPDLOG_INFO("Sub-Reactor add socket {} to epoll loop!", socket_virtual_fd);
    }

    std::thread task([&](){
       std::this_thread::sleep_for(30000ms);
        SPDLOG_INFO("Change it alter OUT to only EPOLL IN");
        struct epoll_event ree{ EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP, {.fd = socket_virtual_fd} };
        epoll_ctl(epollFd, EPOLL_CTL_MOD ,socket_virtual_fd, &ree);
    });

    auto readyCount = epoll_wait(epollFd, epollQueue, 50 , -1);
    for (int i = 0; i < readyCount; ++i) {
        if(epollQueue[i].events & EPOLLERR){
            SPDLOG_INFO("ERROR!!!!");
        }
        if (epollQueue[i].events & EPOLLIN){
            SPDLOG_INFO("EPOLLIN!!!!");
        }
        if (epollQueue[i].events & EPOLLOUT){
            SPDLOG_INFO("EPOLLOUT!!!!");
        }
    }

    task.join();

    return 0;
}