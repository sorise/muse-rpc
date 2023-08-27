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
    Client remix("125.91.127.142 ", 15000, MemoryPoolSingleton());

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

    Transmitter transmitter(14500, GetThreadPoolSingleton());

    std::vector<double> score = {
            100.526,95.84,75.86,99.515,6315.484,944.5,98.2,99898.26,9645.54,484.1456,8974.4654,4894.156,
            89,12,0.56,95.56,78,84,59,68,49,75
    };

    transmitter.start(TransmitterThreadType::Asynchronous);

    transmitter.set_request_timeout(1500);
    transmitter.set_response_timeout(2000);

    TransmitterEvent event("127.0.0.1", 15000);
    event.call<int>("@context/test_ip_client", 15);
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
    transmitter.send(std::move(event));

    std::cin.get();
}

void tes(){
    std::string name{"@context/test_ip_client"};
    BinarySerializer serializer;
    serializer.inputArgs(name);
    std::tuple<int> v(10);
    serializer.input(v);

    std::shared_ptr<char[]> dt( new char[125]);

    std::memcpy(dt.get(),serializer.getBinaryStream(), serializer.byteCount());

    uint32_t client_ip_address = 15;
    uint16_t client_port = 15000;
    muse::serializer::BinarySerializer serializer_;

    serializer_.inputArgs(client_ip_address, client_port);

    std::memcpy((char*)dt.get() + serializer.byteCount(), serializer_.getBinaryStream(), serializer_.byteCount());

    std::string request_name;
    serializer.output(request_name); //获得方法的名称
    auto pos = serializer.getReadPosition() + sizeof(muse::serializer::BinaryDataType);
    auto char_pos = (char*)dt.get() + pos;
    muse::serializer::BinarySerializer::Tuple_Element_Length tpl_size = *reinterpret_cast<uint16_t *>(char_pos); //元祖长度

    tpl_size += 2; // 把数组长度  + 2
    //覆盖原来的数组长度
    std::memcpy(char_pos, (char*)&tpl_size, sizeof(muse::serializer::BinarySerializer::Tuple_Element_Length));
    serializer.clear();
    serializer.write(dt.get(),125);

    std::string name_;

    std::tuple<int , uint32_t, uint16_t> tpl;
    serializer.outputArgs(name_);
    serializer.outputArgs(tpl);

    std::cout << std::get<2>(tpl);
}

int main(int argc, char *argv[]){
    test_v();
    return 0;
}