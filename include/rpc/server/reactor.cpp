//
// Created by remix on 23-7-17.
//
#include "reactor.hpp"

namespace muse::rpc{

    /* 构造函数 */
    Reactor::Reactor(uint16_t _port,uint32_t _sub_reactor_count,uint32_t _open_max_connection, ReactorRuntimeThread _type):
    port(_port),
    type(_type),
    epollFd(-1),
    openMaxConnection(_open_max_connection),
    subReactorCount(_sub_reactor_count),
    runner(nullptr),
    counter(0), //负载均衡策略
    socketFd(-1){
        pool = MemoryPoolSingleton();
    }

    /* 析构函数 */
    Reactor::~Reactor() {
        for (auto &sub: subs) {
            sub->stop();
        }

        runState.store(false, std::memory_order_release);
        if (runner != nullptr){
            if (runner->joinable()) runner->join();
        }
        if (epollFd != -1) close(epollFd);
        if (socketFd != -1) close(socketFd);
        epollFd = -1;
        socketFd = -1;
        SPDLOG_INFO("Main-Reactor end life!");
    }

    void Reactor::loop() {
        //创建 epoll
        if ((epollFd = epoll_create(128)) == -1){
            SPDLOG_ERROR("Main-Reactor loop function epoll fd create failed, errno {}", port, errno);
            throw ReactorException("[Main-Reactor]", ReactorError::EpollFdCreateFailed);
        }
        //创建服务器 socket
        if( (socketFd = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK,IPPROTO_UDP)) == -1 ){
            SPDLOG_ERROR("Main-Reactor loop function socket fd create failed, errno {}", port, errno);
            throw ReactorException("[Main-Reactor]", ReactorError::SocketFdCreateFailed);
        }
        //服务器地址
        sockaddr_in serverAddress{
                PF_INET,
                htons(port),
                htonl(INADDR_ANY)
        };

        int on = 1;
        //地址复用 端口复用
        setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        //地址复用 端口复用
        setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

        //绑定端口号 和 IP
        if(bind(socketFd, (const struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
            SPDLOG_ERROR("socket bind port {} error, errno {}", port, errno);
            throw ReactorException("[Main-Reactor]", ReactorError::SocketBindFailed);
        }
        uint32_t events = EPOLLIN | EPOLLERR | EPOLLHUP| EPOLLRDHUP;

        struct epoll_event ReactorEpollEvent{events, {.fd = socketFd} };

        if( epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &ReactorEpollEvent) == -1 ){
            close(epollFd); //关闭epoll
            close(socketFd); //关闭 socket
            epollFd = -1;
            socketFd = -1;
            SPDLOG_ERROR("Epoll Operation EPOLL_CTL_ADD failed in Line!");
            throw ReactorException("[Main-Reactor]", ReactorError::EpollEPOLL_CTL_ADDFailed);
        }
        struct epoll_event epollQueue[10]; //啥用也没有！！
        //启动
        runState.store(true, std::memory_order_release);
        //启动发射器
        if (is_start_transmitter) start_transmitter();
        //启动从引擎
        startSubReactor(); //失败直接抛出异常
        //开始循环
        while (runState.load(std::memory_order_relaxed)){
            //先处理定时器任务
            {
                std::lock_guard<std::mutex> lock(GlobalEntry::mtx);
                GlobalEntry::timerTree->runTask();
            }
            //处于阻塞状态
            auto readyCount = epoll_wait(epollFd, epollQueue,10 , 10);
            for (int i = 0; i < readyCount; ++i) {
                if (epollQueue[i].events & EPOLLERR){
                    SPDLOG_ERROR("epoll epollQueue errno: {}", errno);
                }
                else if (epollQueue[i].events & EPOLLHUP){
                    SPDLOG_ERROR("epoll event EPOLLHUP Event errno: {}", errno);
                }
                else if (epollQueue[i].events & EPOLLRDHUP){
                    SPDLOG_ERROR("epoll event EPOLLRDHUP Event errno: {}", errno);
                }else{
                    //产生一个缓冲区
                    constexpr unsigned int bufferSize = Protocol::FullPieceSize + 1;
                    void * dp = pool->allocate(bufferSize);
                    std::shared_ptr<char[]> dt((char*)dp, [&](char *ptr){
                        pool->deallocate(ptr, Protocol::FullPieceSize + 1);
                    });
                    //收到新链接
                    sockaddr_in addr{};
                    socklen_t len = sizeof(addr);

                    //读取数据
                    auto recvLen = recvfrom(epollQueue[i].data.fd, dp, bufferSize, 0 ,(struct sockaddr*)&addr, &len);
                    SPDLOG_INFO("Main-Reactor receive new udp connection from ip {} port {}" ,inet_ntoa(addr.sin_addr)  ,ntohs(addr.sin_port));

                    //判断是否能够解析成功
                    bool isSuccess = false;
                    auto header = protocol.parse(dt.get(), recvLen, isSuccess);
                    if (!isSuccess){
                        //解析失败、协议格式不正确
                        std::string message {"Please use the correct network protocol！\n"};
                        sendto(socketFd, message.c_str() , sizeof(char)*message.size(),0, (struct sockaddr*)&addr, sizeof(addr));
                        continue; //直接下一个
                    }
                    if (transmitter_linker != nullptr){
                        Servlet servlet(header.timePoint,ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr));
                        if(GlobalEntry::active_queue->count(servlet) > 0){
                            transmitter_linker->acceptNewData(header.timePoint, dt, recvLen, addr);
                            continue; //下一个
                        }
                    }
                    //解析成功，创建 一个二元组
                    Peer peer(ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr));
                    //查找
                    auto it= GlobalEntry::con_queue->find(peer);
                    if (it != GlobalEntry::con_queue->end()){
                        //已经创建连接了
                        subs[it->second.SubReactor_index]->acceptConnection(it->second.socket, recvLen, addr, dt, false);
                        SPDLOG_INFO("directly send data to sub reactor because vir connection is build!");
                        continue;
                    }
                    //真新链接、建立链接
                    int sonSocketFd = -1;
                    if ((sonSocketFd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
                        SPDLOG_WARN("Main-Reactor create connect udp socket failed， errno: {}", errno);
                    }
                    else {
                        //地址、端口复用 端口复用
                        setsockopt(sonSocketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
                        setsockopt(sonSocketFd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

                        //绑定端口号 和 IP
                        if(bind(sonSocketFd, (const struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
                            SPDLOG_ERROR("son socket bind port {} failed, errno {}", port, errno);
                        }
                        // addr 是 客户端地址
                        if (connect(sonSocketFd, (struct sockaddr *) &addr, sizeof(struct sockaddr)) == -1) {
                            SPDLOG_ERROR("son socket connect ip {} port {} failed, errno {}",inet_ntoa(addr.sin_addr), ntohs(port), errno);
                        } else {
                            //成功链接，丢给 从反应堆
                            uint32_t idx = counter % subReactorCount;
                            counter++;
                            SPDLOG_INFO("Main-Reactor send connection to {} sub reactor",idx);
                            auto result = subs[idx]->acceptConnection(sonSocketFd, recvLen, addr, dt, true);
                            if (!result){
                                SPDLOG_WARN("Sub Reactor has been stopped!");
                                close(sonSocketFd);
                            }else{
                                GlobalEntry::con_queue->insert_or_assign(peer, SocketConnection{sonSocketFd, idx} );
                            }
                        }
                    }
                }
            }
        }
    }


    void Reactor::stop() noexcept {
        runState.store(false, std::memory_order_release);
        if (runner != nullptr){
            //关闭主反应堆
            if (runner->joinable()) runner->join();
        }
        //关闭从反应堆
        for (auto &sub: subs ) {
            sub->stop();
        }
    }

    void Reactor::start(bool _is_start_transmitter){
        this->is_start_transmitter = _is_start_transmitter;
        //首先创建 sub_reactor
        try {
            for (int i = 0; i < subReactorCount; ++i) {
                subs.emplace_back(std::make_unique<SubReactor>(port, openMaxConnection, pool));
            }
        }catch(std::exception &exp){
            SPDLOG_ERROR("Main-Reactor create sub reactor failed, memory not enough, exception what:{}!", exp.what());
            throw ReactorException("[Main-Reactor]", ReactorError::CreateSubReactorFailed);
        }
        //启动主引擎
        SPDLOG_INFO("Reactor server start to run in port {} connection limit {} and run type Synchronous", port, openMaxConnection);
        if (type == ReactorRuntimeThread::Synchronous){
            loop();
        }else{
            try {
                runner = std::make_shared<std::thread>(&Reactor::loop, this);
            }catch(std::exception &exp){
                throw ReactorException("[Main-Reactor]", ReactorError::CreateMainReactorThreadFailed);
            }
        }
    }

    void Reactor::startSubReactor() {
        for (int i = 0; i < subReactorCount; ++i) {
            if (subs[i] != nullptr){
                subs[i]->start();
            }
        }
    }

    bool Reactor::send(TransmitterEvent && event) {
        if (transmitter_linker != nullptr && runState.load(std::memory_order_relaxed) ){
            sockaddr_in server_address {};
            if(1 != inet_aton(event.get_ip_address().c_str(),&server_address.sin_addr))
                throw ClientException("ip address not right", ClientError::IPAddressError);

            auto msg_id = GlobalSecondsId();
            Servlet servlet(msg_id,  event.port,  ntohl(server_address.sin_addr.s_addr));
            {
                std::lock_guard<std::mutex> lock(GlobalEntry::mtx_active_queue);
                GlobalEntry::active_queue->insert(servlet);
            }
            transmitter_linker->send(std::forward<TransmitterEvent&&>(event), msg_id);
            return true;
        }
        return false;
    }

    void Reactor::start_transmitter() {
        if (transmitter_linker == nullptr){
            try{
                transmitter_linker = std::make_unique<TransmitterLinkReactor>(this->socketFd,GetThreadPoolSingleton());
                if (transmitter_linker != nullptr){
                    //启动 发射器
                    transmitter_linker->start(TransmitterThreadType::Asynchronous); //异步，启动
                }
            } catch (std::exception &exp) {
                throw ReactorException("[Main-Reactor]", ReactorError::CreateTransmitterThreadFailed);
            }

        }
    }

    void Reactor::wait_transmitter() {
        //没有启动发射器 直接返回
        if (!is_start_transmitter) return;
        while ( transmitter_linker == nullptr || !transmitter_linker->start_finish()){
            std::this_thread::sleep_for(10ms);
        }
    }
}