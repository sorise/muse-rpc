//
// Created by remix on 23-7-17.
//
#include "reactor.hpp"
#include "spdlog/spdlog.h"

namespace muse::rpc{

    /* 构造函数 */
    Reactor::Reactor(short _port, ReactorRuntimeThread _type, int _maxConnection):
    port(_port), type(_type), openMaxConnection(_maxConnection), epollFd(-1), runner(nullptr),socketFd(-1){}

    /* 析构函数 */
    Reactor::~Reactor() {
        if (runner != nullptr){
            if (runner->joinable()) runner->join();
        }
        if (epollFd != -1) close(epollFd);
        if (socketFd != -1) close(socketFd);
        SPDLOG_INFO("Reactor end life!");
    }

    bool Reactor::loop() {
        //创建 epoll
        if ((epollFd = epoll_create(128)) == -1){
            return false;
        }
        //创建服务器 socket
        if( (socketFd = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK,IPPROTO_UDP)) == -1 ){
            return false;
        }
        //服务器地址
        sockaddr_in serverAddress{
                AF_INET,
                htons(port),
                htonl(INADDR_ANY)
        };

        //绑定端口号 和 IP
        if(bind(socketFd, (const struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
            SPDLOG_ERROR("socket bind port {} error, errno {}", port, errno);
            return false;
        }

        int on = 1;
        //地址复用 端口复用
        setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        //地址复用 端口复用
        setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

        uint32_t events = EPOLLIN | EPOLLERR | EPOLLHUP| EPOLLRDHUP;

        struct epoll_event ReactorEpollEvent{events, {.fd = socketFd} };

        if( epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &ReactorEpollEvent) == -1 ){
            SPDLOG_ERROR("Epoll Operation EPOLL_CTL_ADD failed in Line!");
            close(epollFd); //关闭epoll
            close(socketFd); //关闭 socket
            epollFd = -1;
            socketFd = -1;
            return false;
        }
        struct epoll_event epollQueue[openMaxConnection];
        //启动
        runState.store(true, std::memory_order_release);
        //产生一个缓冲区
        constexpr unsigned int bufferSize = Protocol::defaultPieceLimitSize + 1;
        char recvBuf[bufferSize] = { '\0' };
        //开始循环
        while (runState.load(std::memory_order_relaxed)){
            //处于阻塞状态
            auto readyCount = epoll_wait(epollFd, epollQueue,openMaxConnection , -1);
            for (int i = 0; i < readyCount; ++i) {
                if (epollQueue[i].events & EPOLLERR){
                    SPDLOG_ERROR("epoll epollQueue errno: {}", errno);
                    throw std::runtime_error("exit"); //停掉服务直接退出
                }
                else if (epollQueue[i].events & EPOLLHUP){
                    SPDLOG_ERROR("epoll event EPOLLHUP Event errno: {}", errno);
                    throw std::runtime_error("exit"); //停掉服务直接退出
                }
                else if (epollQueue[i].events & EPOLLRDHUP){
                    SPDLOG_ERROR("epoll event EPOLLRDHUP Event errno: {}", errno);
                    throw std::runtime_error("exit"); //停掉服务直接退出
                }else{
                    //收到新链接
                    sockaddr_in addr{};
                    socklen_t len = sizeof(addr);
                    //读取数据
                    auto recvLen = recvfrom(epollQueue[i].data.fd, recvBuf, bufferSize, 0 ,(struct sockaddr*)&addr, &len);
                    SPDLOG_INFO("Main-Reactor receive new udp connection from ip {} port {}" ,inet_ntoa(addr.sin_addr)  ,ntohs(addr.sin_port));
                    if (recvLen > 0){
                        bool isSuccess = false;
                        auto header = Protocol::parse(recvBuf, recvLen, isSuccess);
                        if (isSuccess){
                            //读取成功 上交报文，建立链接




                        }else{
                            //协议格式不正确
                           std::string message {"Please use the correct network protocol！\n"};
                           sendto(socketFd, message.c_str() , sizeof(char)*message.size(),0, (struct sockaddr*)&addr, len);
                        }
                    }else{
                        SPDLOG_WARN("main Reactor receive 0 bytes data from ip {} port {} errno {}" ,inet_ntoa(addr.sin_addr)  ,ntohs(addr.sin_port), errno);
                    }
                }
            }
        }
        return true;
    }

    void Reactor::stop() noexcept {
        runState.store(false, std::memory_order_release);
        if (runner != nullptr){
            if (runner->joinable()) runner->join();
        }
    }

    bool Reactor::start() noexcept {
        if (type == ReactorRuntimeThread::Synchronous){
            SPDLOG_INFO("Reactor server start to run in port {} connection limit {} and run type Synchronous", port, openMaxConnection);
            return loop();
        }else{
            SPDLOG_INFO("Reactor server start to run in port {} connection limit {} and run type Asynchronous", port, openMaxConnection);
            runner = std::make_shared<std::thread>(&Reactor::loop, this);
        }
        return false;
    }
}