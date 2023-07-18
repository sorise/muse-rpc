//
// Created by remix on 23-7-17.
//
#include <atomic>
#ifndef MUSE_SERVER_REACTOR_HPP
#define MUSE_SERVER_REACTOR_HPP
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory>
#include <thread>
#include <iostream>
#include <arpa/inet.h>
#include "../protocol/conf.hpp"
#include "../../logger/conf.hpp"

namespace muse::rpc{
    enum class ReactorRuntimeThread: int {
        // 同步方式，反应堆在当前线程运行
        Synchronous = 1,
        // 反应堆新开一个线程运行
        Asynchronous = 2
    };

    class Reactor{
    private:
        short port;
        int openMaxConnection;
        int epollFd;
        int socketFd;
        std::atomic<bool> runState {false};
        bool loop();
        std::shared_ptr<std::thread> runner;
        ReactorRuntimeThread type;
    public:
        Reactor(short _port,
                ReactorRuntimeThread _type,
                int _maxConnection
        );
        bool start() noexcept;
        void stop() noexcept;
        virtual ~Reactor();
    };
}
#endif //MUSE_SERVER_REACTOR_HPP
