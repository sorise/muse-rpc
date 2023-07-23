#include <iostream>
#include <random>
#include <exception>
#include <chrono>
#include <zlib.h>
#include <map>
#include "timer/timer_wheel.hpp"

#include "rpc/protocol/protocol.hpp"

#include "rpc/server/reactor.hpp"
#include "logger/conf.hpp"
#include "rpc/server/reactor.hpp"
#include <functional>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rpc/server/reqeust.hpp"

using namespace muse::logger;
using namespace muse::rpc;
using namespace muse::rpc;
using namespace std::chrono_literals;

int main() {
    InitSystemLogger(); //启动日志

    Reactor reactor(15000, 1,1500, ReactorRuntimeThread::Asynchronous);

    try {
        reactor.start();
    }catch (const ReactorException &ex){
        SPDLOG_ERROR("Main-Reactor start failed!");
    }
    std::cin.get();

    spdlog::default_logger()->flush();
}
