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
#include "serializer/binaryDeserializeView.hpp"
#include "serializer/binarySerializer.h"

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



std::vector<double> test_fun2(const std::vector<double>& scores){
    std::vector<double> tr = {
          89,78,58,94,15,65,94,100,12.52
    };
    return tr;
}

int read_str(std::string message, const std::vector<double>& scores){
    return (int)message.size();
}

int test_fun1(int value){
    int i  = 10;
    return i + value;
}

// @context/test_ip_client
int get_client_ip_port(int age, uint32_t client_ip_address, uint16_t client_port){
    std::cout << "client >>" << client_ip_address <<":" << client_port << std::endl;
    return 100 + age;
}

// @context/test_ip_client_test
int test_client_ip_port(uint32_t client_ip_address, uint16_t client_port){
    std::cout << "client >>" << client_ip_address <<":" << client_port << std::endl;
    return 100;
}


int main(){
    muse::rpc::Disposition::Server_Configure(4, 4, 4096, 3000ms, "/home/remix/log");

    muse_bind_sync( Disposition::Prefix +"test_ip_client", get_client_ip_port);
    muse_bind_sync( Disposition::Prefix + "test_ip_client_test", test_client_ip_port);

    muse_bind_async("lambda test", [](int val)->int{
        printf("why call me \n");
        return 10 + val;
    });

    // 开一个线程启动反应堆,等待请求
    Reactor reactor(15000, 2,2000, ReactorRuntimeThread::Asynchronous);

    //开始运行
    try {
        reactor.start(false);
    }catch (const ReactorException &ex){
        SPDLOG_ERROR("Main-Reactor start failed!");
    }

    std::cin.get();
    spdlog::default_logger()->flush(); //刷新日志
}
