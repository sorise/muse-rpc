#include <iostream>
#include <random>
#include <exception>
#include <chrono>
#include <zlib.h>
#include "rpc/protocol/conf.hpp"
#include "timer/timer_wheel.hpp"
#include "rpc/server/reactor.hpp"
#include "logger/conf.hpp"
#include "rpc/server/reactor.hpp"


using namespace muse::logger;
using namespace muse::rpc;
using namespace std::chrono_literals;

int main() {
    InitSystemLogger(); //启动日志

    Reactor reactor(15000, ReactorRuntimeThread::Asynchronous, 400);
    reactor.start();

    std::cin.get();
}
