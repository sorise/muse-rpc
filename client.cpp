#include <iostream>
#include "rpc/client/invoker.hpp"
#include "rpc/client/client.hpp"
#include "rpc/client/response_data.hpp"
#include "rpc/client/transmitter.hpp"
#include "rpc/memory/conf.hpp"
#include "rpc/rpc.hpp"

using namespace muse::rpc;
using namespace muse::timer;
using namespace std::chrono_literals;

void test_v(){
    //注册中间件
    muse::rpc::Disposition::Client_Configure();

    //方法的路由
    Client remix("110.41.82.121", 15000, MemoryPoolSingleton());
    //调用远程方法
    Outcome<int> result = remix.call<int>("@context/test_ip_client_test");

    if (result.isOK()){
        std::cout << "result: " <<result.value << std::endl;
    }else{
        if (result.protocolReason == FailureReason::OK){
            std::printf("rpc error\n");
        }else{
            std::printf("protocol error\n");
        }
    }
}

int main(int argc, char *argv[]){
    test_v();
    return 0;
}