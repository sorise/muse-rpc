//
// Created by remix on 23-7-19.
//
#include "sub_reactor.hpp"

namespace muse::rpc{


    void SubReactor::acceptConnection(int _socketFd, size_t _recv_length, sockaddr_in addr, const std::shared_ptr<char[]>& data) {
        std::lock_guard<std::mutex> lock(newConLocker);
        newConnections.emplace(_socketFd, _recv_length, addr,  data);
    }


    SubReactor::SubReactor(uint16_t _port ,int _open_max_connection, std::shared_ptr<std::pmr::synchronized_pool_resource> _pool):
    openMaxConnection(_open_max_connection),
    port(_port),
    epollFd(-1),
    runner(nullptr){
        //内存池
        pool = _pool;
        connections = new VirtualConnection[openMaxConnection]; //链接槽
    }


    void SubReactor::start() {
        try {
            runner = std::make_shared<std::thread>(&SubReactor::loop, this);
        }catch(std::bad_alloc &ex){
            SPDLOG_ERROR("Main-Reactor start function failed, memory not enough!");
        }catch(std::exception &exp){
            SPDLOG_ERROR("Main-Reactor start function failed, exception what:{}!", exp.what());
        }
    }

    void SubReactor::loop() {
        SPDLOG_INFO("SubReactor Start run ");
        //创建 epoll
        if ((epollFd = epoll_create(128)) == -1){
            SPDLOG_ERROR("Sub-Reactor loop function failed epoll fd created failed!");
            return;
        }
        struct epoll_event epollQueue[this->openMaxConnection]; //啥用也没有！！
        //启动
        runState.store(true, std::memory_order_release);
        //产生一个缓冲区
        constexpr unsigned int bufferSize = Protocol::FullPieceSize + 1;
        char recvBuf[bufferSize] = { '\0' };
        //启动从引擎
        while (runState.load(std::memory_order_relaxed)){
            //处于阻塞状态
            auto readyCount = epoll_wait(epollFd, epollQueue,10 , 10);
            auto now = GetNowTick();
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
                }
                else if (epollQueue[i].events & EPOLLIN){
                    //收到新链接
                    sockaddr_in addr{};
                    socklen_t len = sizeof(addr);
                    auto vc = static_cast<VirtualConnection*>( epollQueue[i].data.ptr );
                    vc->last_active = now;
                    //读取数据
                    auto recvLen = recvfrom(vc->socket_fd, recvBuf, bufferSize, 0 ,(struct sockaddr*)&addr, &len);
                    if (recvLen > 0){
                        bool isSuccess = false;
                        auto header = protocol_util.parse(recvBuf, recvLen, isSuccess);
                        // 读取成功 上交报文，建立链接
                        if (isSuccess){
                            SPDLOG_INFO("Sub-Reactor accept data message id {} ip {} port {} ",header.timePoint, inet_ntoa(addr.sin_addr), htons(addr.sin_port));
                            //处理 Request 阶段
                            if (header.phase == CommunicationPhase::Request){
                                //这是数据报文
                                Servlet base(header.timePoint, ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr));
                                auto it= messages.find(base);
                                uint16_t  ACK = 0; //确认号
                                if (header.type == ProtocolType::RequestSend){
                                    if (it != messages.end()){
                                        /* 旧数据，如果是非重复数据*/
                                        if(!it->second.getPieceState(header.pieceOrder)){
                                            char * des = (char *)(it->second.data.get()) + (header.pieceStandardSize * header.pieceOrder);
                                            // 完成报文内容拷贝
                                            std::memcpy(des, recvBuf ,header.pieceSize);
                                            ACK = it->second.setPieceState(header.pieceOrder, true);
                                        }
                                    }else{
                                        /* 需要新建一个 request */
                                        Request request(header.timePoint, ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr), header.totalCount, header.totalSize);
                                        request.setSocket(epollQueue[i].data.fd);
                                        ACK = request.setPieceState(header.pieceOrder, true);
                                        auto store = pool->allocate(header.totalSize);
                                        std::shared_ptr<char[]> dt((char*)store, [&, total=header.totalSize](char *ptr){
                                            SPDLOG_INFO("store release {}!", total);
                                            pool->deallocate(ptr, total);
                                        });
                                        char * des = (char *)store + (header.pieceStandardSize * header.pieceOrder);
                                        // 完成报文内容拷贝
                                        std::memcpy(des, recvBuf + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
                                        request.data = dt;
                                        messages.emplace(base, request);
                                    }
                                    sendACK(vc->socket_fd,ACK, header.timePoint);
                                }
                                else if (header.type == ProtocolType::RequestACK){
                                    if (it != messages.end()){
                                        sendACK(vc->socket_fd, it->second.getAckNumber(), header.timePoint);
                                    }else{
                                        sendReset(vc->socket_fd, header.timePoint);
                                    }
                                }
                                else if(header.type == ProtocolType::TimedOutRequestHeartbeat){
                                    if (it != messages.end()){
                                        sendHeartbeat(vc->socket_fd, header.timePoint);
                                    }else{
                                        sendReset(vc->socket_fd, header.timePoint);
                                    }
                                }else{
                                    sendReset(vc->socket_fd, header.timePoint);
                                }
                            }
                            else if(header.phase == CommunicationPhase::Response){
                                //二阶段 收到确认的 ACK
                                if (header.type == ProtocolType::ReceiverACK){

                                }
                            }
                        }
                        else{
                            SPDLOG_ERROR("accept error protocol data ip {} port {} ",inet_ntoa(addr.sin_addr), htons(addr.sin_port));
                            //协议格式不正确
                            std::string message {"Please use the correct network protocol！\n"};
                            write(vc->socket_fd, message.c_str() , sizeof(char)*message.size());
                        }
                    }
                    else{
                        SPDLOG_WARN("main Reactor receive 0 bytes data from ip {} port {} errno {}" ,inet_ntoa(addr.sin_addr)  ,ntohs(addr.sin_port), errno);
                    }
                    vc = nullptr;
                }
            }
            dealWithNewConnection(); //处理新的链接
            //处理发送任务

        }
    }

    /* 处理新链接 ------- ------- -------- -------- */
    void SubReactor::dealWithNewConnection() {
        //进行互斥处理
        std::lock_guard<std::mutex> lock(newConLocker);
        //有多少新的链接
        size_t newConnectionCount = newConnections.size();
        for (int i = 0; i < newConnectionCount; ++i) {
            //新链接的 socket
            int _socket_fd = std::get<0>(newConnections.front());
            //数据长度
            size_t _recv_length = std::get<1>(newConnections.front());
            //地址信息
            sockaddr_in addr = std::get<2>(newConnections.front());
            //复制的数据
            std::shared_ptr<char[]> data = std::get<3>(newConnections.front());

            auto findResult = findConnectionsEmptyPlace();

            if (!findResult.first){
                sendExhausted(_socket_fd);
                close(_socket_fd);
                continue; //下一个 好
            }
            connections[findResult.second].socket_fd = _socket_fd;
            connections[findResult.second].last_active = GetNowTick();
            connections[findResult.second].addr = addr;

            activeConnectionsCount++;

            uint32_t events = EPOLLIN |EPOLLERR | EPOLLHUP| EPOLLRDHUP;
            VirtualConnection *vc = connections + findResult.second;
            struct epoll_event ReactorEpollEvent{events, {.ptr = vc} };

            if( epoll_ctl(epollFd, EPOLL_CTL_ADD, _socket_fd, &ReactorEpollEvent) == - 1 ){
                close(_socket_fd); //关闭 socket
                SPDLOG_ERROR("Epoll Operation EPOLL_CTL_ADD failed in Sub-Reactor deal with new Connection errno {} !", errno);
                continue;
            }else{
                SPDLOG_INFO("Sub-Reactor add socket {} to epoll loop!", _socket_fd);
            }

            bool isSuccess = false;
            auto header = protocol_util.parse(data.get(), _recv_length, isSuccess);

            if (isSuccess){
                SPDLOG_INFO("Sub-Reactor parse data ok!");
                // 读取成功 上交报文，建立链接
                //协议类型
                Servlet requestBase(header.timePoint, ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr));
                //看看当前报文存不存在
                auto it= messages.find(requestBase);
                if (header.type == ProtocolType::RequestSend){
                    uint16_t  ACKNumber = 0; //确认号
                    if (it != messages.end()){
                        //有历史报文
                        if(!it->second.getPieceState(header.pieceOrder)){
                            char * des = (char *)(it->second.data.get()) + (header.pieceStandardSize * header.pieceOrder);
                            // 完成报文内容拷贝
                            std::memcpy(des, data.get() + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
                            ACKNumber = it->second.setPieceState(header.pieceOrder, true);
                        }
                    }
                    else
                    {
                        //新的历史报文
                        Request request(header.timePoint, ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr), header.totalCount, header.totalSize);
                        request.setSocket(_socket_fd);
                        ACKNumber = request.setPieceState(header.pieceOrder, true);

                        auto store = pool->allocate(header.totalSize);
                        std::shared_ptr<char[]> dt((char*)store, [&, total=header.totalSize](char *ptr){
                            pool->deallocate(ptr, total);
                        });
                        request.data = dt;
                        char * des = (char *)store + (header.pieceStandardSize * header.pieceOrder);
                        // 完成报文内容拷贝
                        std::memcpy(des, data.get() + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
                        messages.emplace(requestBase, request);
                    }
                    sendACK(_socket_fd, ACKNumber, header.timePoint);
                }else if(header.type == ProtocolType::RequestACK){
                    if (it != messages.end()){
                        sendACK(_socket_fd, it->second.getAckNumber(), header.timePoint);
                    }else{
                        sendReset(_socket_fd, header.timePoint);
                    }
                }
                else if(header.type == ProtocolType::TimedOutRequestHeartbeat){
                    if (it != messages.end()){
                        sendHeartbeat(_socket_fd, header.timePoint);
                    }else{
                        sendReset(_socket_fd, header.timePoint);
                    }
                }else{
                    //除此以外 全部发送链接已经重置
                    sendReset(_socket_fd, header.timePoint);
                }
            }else{
                //协议格式不正确
                std::string message {"Please use the correct network protocol！\n"};
                sendto(_socket_fd, message.c_str() , sizeof(char)*message.size(),0, (struct sockaddr*)&addr, sizeof(addr));
            }
            newConnections.pop();
        }
    }

    SubReactor::~SubReactor() {
        runState.store(false, std::memory_order_release);
        //等待线程结束
        if (runner != nullptr){
            if (runner->joinable()) runner->join();
        }
        if (epollFd != -1) close(epollFd);

        delete [] connections;
        SPDLOG_INFO("Main-Reactor end life!");
    }

    /* 对过去数据的确认 */
    void SubReactor::sendACK(int _socket_fd, uint16_t _ack_number,  uint64_t _message_id) {
        //发送ACK
        char ACKBuffer[muse::rpc::Protocol::protocolHeaderSize];
        protocol_util.initiateSenderProtocolHeader(
                ACKBuffer,
                _message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol_util.setProtocolType(
                ACKBuffer,
                ProtocolType::ReceiverACK
        );
        protocol_util.setACKAcceptOrder(ACKBuffer, _ack_number);
        auto result = write(_socket_fd, ACKBuffer,muse::rpc::Protocol::protocolHeaderSize);
        if (result == -1){
            SPDLOG_ERROR("Sub-Reactor Send the ACK {} failed {} Protocol timePoint {}" ,_ack_number, _message_id);
        }else{
            SPDLOG_INFO("Sub-Reactor Send the ACK {} successfully Protocol timePoint {}", _ack_number, _message_id);
        }
    }

    //标识当前链接已经被重置了
    void SubReactor::sendReset(int _socket_fd, uint64_t _message_id) {
        char buffer[muse::rpc::Protocol::protocolHeaderSize];
        protocol_util.initiateSenderProtocolHeader(
                buffer,
                _message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol_util.setProtocolType(
                buffer,
                ProtocolType::StateReset
        );
        auto result = write(_socket_fd, buffer,muse::rpc::Protocol::protocolHeaderSize);
        if (result == -1){
            SPDLOG_ERROR("Sub-Reactor Send the StateReset failed Protocol timePoint {}" ,_message_id);
        }else{
            SPDLOG_INFO("Sub-Reactor Send the StateReset successfully Protocol timePoint {}", _message_id);
        }
    }
    //发送心跳信息
    void SubReactor::sendHeartbeat(int _socket_fd, uint64_t _message_id) {
        char buffer[muse::rpc::Protocol::protocolHeaderSize];
        protocol_util.initiateSenderProtocolHeader(
                buffer,
                _message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol_util.setProtocolType(
                buffer,
                ProtocolType::TimedOutResponseHeartbeat
        );
        auto result = write(_socket_fd, buffer,muse::rpc::Protocol::protocolHeaderSize);
        if (result == -1){
            SPDLOG_ERROR("Sub-Reactor Send the Response Heart beat failed Protocol timePoint {}" ,_message_id);
        }else{
            SPDLOG_INFO("Sub-Reactor Send the Response Heart beat successfully Protocol timePoint {}", _message_id);
        }
    }
    //发送服务器资源已经耗尽
    void SubReactor::sendExhausted(int _socket_fd) {
        char buffer[muse::rpc::Protocol::protocolHeaderSize];
        protocol_util.initiateSenderProtocolHeader(
                buffer,
                0,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol_util.setProtocolType(
                buffer,
                ProtocolType::TheServerResourcesExhausted
        );
        auto result = write(_socket_fd, buffer,muse::rpc::Protocol::protocolHeaderSize);
        if (result == -1){
            SPDLOG_ERROR("Sub-Reactor Send TheServerResourcesExhausted failed Protocol timePoint ");
        }else{
            SPDLOG_INFO("Sub-Reactor Send TheServerResourcesExhausted successfully Protocol timePoint ");
        }
    }

    std::pair<bool, size_t> SubReactor::findConnectionsEmptyPlace() {
        std::pair<bool, size_t> result { false, 0 };
        size_t idx = 0;
        for (int i = 0; i < openMaxConnection; ++i) {
            idx = (lastEmptyPlace + i) % openMaxConnection;
            if (connections[idx].socket_fd == -1 ){
                result.first = true;
                result.second = idx;
                break;
            }
            lastEmptyPlace ++;
        }
        return result;
    }

    int SubReactor::clearTimeoutConnections() {
        int clearCount = 0;
        auto now = GetNowTick();
        for (int i = 0; i < openMaxConnection; ++i) {
            if (connections[i].socket_fd != -1 ){
                if (now -connections[i].last_active > SubReactor::ConnectionTimeOut){
                    connections[i].socket_fd = -1;
                    clearCount++;
                    this->activeConnectionsCount--;
                }
            }
        }
        return clearCount;
    }
}
