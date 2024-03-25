//
// Created by remix on 23-7-19.
//
#include "sub_reactor.hpp"

#include <utility>

namespace muse::rpc{

    bool SubReactor::acceptConnection(int _socketFd, size_t _recv_length, sockaddr_in addr, const std::shared_ptr<char[]>& data , bool new_connection) {
        if (this->runState.load(std::memory_order_relaxed)){
            {
                //加入队列中
                std::lock_guard<std::mutex> lock(newConLocker);
                newConnections.emplace(_socketFd, _recv_length, addr,  data,new_connection);
            }
            //唤醒 epoll
            struct epoll_event ree{ EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP, {.fd = epoll_switch_fd} };
            epoll_ctl(epollFd, EPOLL_CTL_MOD ,epoll_switch_fd, &ree);
            return true;
        }
        return false;
    }

    SubReactor::SubReactor(uint16_t _port ,int _open_max_connection, std::shared_ptr<std::pmr::synchronized_pool_resource> _pool):
    openMaxConnection(_open_max_connection),
    port(_port),
    epollFd(-1),
    pool(std::move(_pool)),
    runner(nullptr){
        //内存池
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
        //创建 epoll
        if ((epollFd = epoll_create(128)) == -1){
            SPDLOG_ERROR("Sub-Reactor loop function failed epoll fd created failed!");
            return;
        }
        struct epoll_event epollQueue[this->openMaxConnection];
        //启动
        runState.store(true, std::memory_order_release);
        //缓冲区
        constexpr unsigned int bufferSize = Protocol::FullPieceSize + 1;
        //接受缓存区
        char recvBuf[bufferSize] = { '\0' };

        epoll_switch_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (epoll_switch_fd == -1){
            SPDLOG_ERROR("Sub Reactor epoll epoll_switch_fd create failed!");
            return;
        }else{
            //开关加入
            struct epoll_event ree{ EPOLLIN, {.fd = epoll_switch_fd} };
            if( epoll_ctl(epollFd, EPOLL_CTL_ADD, epoll_switch_fd, &ree) == - 1 ){
                close(epoll_switch_fd); //关闭 socket
                SPDLOG_ERROR("Epoll Operation EPOLL_CTL_ADD failed in Sub-Reactor deal with new Connection errno {} !", errno);
            }else{
                SPDLOG_INFO("Sub-Reactor start loop!", epoll_switch_fd);
            }
        }
        while (runState.load(std::memory_order_relaxed)){
            auto timer_out = treeTimer.checkTimeout();
            auto readyCount = epoll_wait(epollFd, epollQueue,openMaxConnection , timer_out);
            //处理定时器，执行一些因为延迟还未执行的定时任务，避免非法数据驻留
            {
                //处理发送任务
                std::lock_guard<std::mutex> lock(treeTimer_mtx);
                treeTimer.runTask();
            }
            //处理新的链接
            dealWithNewConnection();
            //获得当前时间 毫秒
            auto now = GetNowTick();
            for (int i = 0; i < readyCount; ++i) {
                if(epollQueue[i].events & EPOLLERR){
                    auto cv = (VirtualConnection *)epollQueue[i].data.ptr;
                    closeSocket(cv);
                    SPDLOG_ERROR("epoll event EPOLLERR errno: {} {} {}", errno, cv->socket_fd, (uint32_t)epollQueue[i].events);
                    //throw std::runtime_error("exit"); //停掉服务直接退出
                }
                if (epollQueue[i].events & EPOLLIN){
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
                            //处理 Request 阶段
                            Servlet base(header.timePoint, ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr));
                            auto it= messages.find(base);
                            if (it != messages.end()) {
                                it->second.active_dt = now;
                            }
                            if (header.phase == CommunicationPhase::Request){
                                SPDLOG_INFO("Sub-Reactor accept message request from id [{}] ip [{}] port [{}]", header.timePoint, inet_ntoa(addr.sin_addr), htons(addr.sin_port));
                                //这是数据报文
                                message_queue::iterator ite;
                                if (header.type == ProtocolType::RequestSend){
                                    uint16_t ACK = 0; //确认号
                                    if (it != messages.end()){
                                        /* 已经存储了message 的消息， 如果是非重复数据,需要保存*/
                                        if(!it->second.getPieceState(header.pieceOrder)){
                                            char * des = (char *)(it->second.data.get()) + (header.pieceStandardSize * header.pieceOrder);
                                            // 完成报文内容拷贝
                                            std::memcpy(des, recvBuf + Protocol::protocolHeaderSize ,header.pieceSize);
                                            ACK = it->second.setPieceState(header.pieceOrder, true);
                                        } else{
                                            //重复数据，获得 ACK
                                            ACK = it->second.getAckNumber();
                                        }
                                        ite = it;
                                    }else{
                                        /* 需要新建一个 request */
                                        Request request(header.timePoint, ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr), header.totalCount, header.totalSize);
                                        request.setSocket(epollQueue[i].data.fd);
                                        request.active_dt = now;
                                        ACK = request.setPieceState(header.pieceOrder, true);
                                        auto store = pool->allocate(header.totalSize);
                                        std::shared_ptr<char[]> dt((char*)store, [&, total=header.totalSize](char *ptr){
                                            pool->deallocate(ptr, total);
                                        });
                                        char * des = (char *)store + (header.pieceStandardSize * header.pieceOrder);
                                        // 完成报文内容拷贝
                                        std::memcpy(des, recvBuf + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
                                        request.data = dt;
                                        auto Pair = messages.emplace(base, request);
                                        ite = Pair.first;
                                        setRequestTimeOutEvent(base);
                                    }
                                    sendACK(vc->socket_fd,ACK, header.timePoint);
                                    if (ACK == header.totalCount){ //最后一个了
                                        SPDLOG_INFO("Sub Reactor accept all data of the message_id {}", header.timePoint);
                                        vc->messages.push_back(header.timePoint); //存储ID ????
                                        if (!ite->second.getTriggerState()){
                                            ite->second.trigger();
                                            trigger(ite->second.getTotalDataSize(), ite->second.data ,ite->first, vc);
                                        }
                                    }
                                }
                                else if (header.type == ProtocolType::RequestACK){
                                    if (it != messages.end()){
                                        sendACK(vc->socket_fd, it->second.getAckNumber(), header.timePoint);
                                    }else{
                                        sendReset(vc->socket_fd, header.timePoint, header.phase);
                                    }
                                }
                                else if(header.type == ProtocolType::TimedOutRequestHeartbeat){
                                    if (it != messages.end()){
                                        sendHeartbeat(vc->socket_fd, header.timePoint, header.phase);
                                    }else{
                                        sendReset(vc->socket_fd, header.timePoint, header.phase);
                                    }
                                }else{
                                    sendReset(vc->socket_fd, header.timePoint,header.phase);
                                }
                            }
                            else if(header.phase == CommunicationPhase::Response){
                                SPDLOG_INFO("Sub-Reactor accept message response from id [{}] ip [{}] port [{}]", header.timePoint, inet_ntoa(addr.sin_addr), htons(addr.sin_port));
                                //收到了二阶段的响应信息，先看这个响应信息有没有
                                auto result = std::find_if(vc->messages.begin(), vc->messages.end(), [=](auto res)->bool {
                                    return res == header.timePoint;
                                });
                                //没有找到
                                if (result == vc->messages.end())
                                {
                                    SPDLOG_INFO("Sub-Reactor Send the StateReset successfully Protocol timePoint {} {} {}", header.timePoint, vc->socket_fd, vc->messages.size());
                                    sendReset(vc->socket_fd, header.timePoint, CommunicationPhase::Response); //没有这个消息
                                }
                                else
                                {
                                    //找到这个响应消息，查看是数据是否准备好
                                    auto it_response = vc->responses.find(header.timePoint);
                                    //如果没有准备好
                                    if (it_response == vc->responses.end())
                                    {
                                        //只能发送心跳数据，说明服务器正在处理中，还没有准备好响应数据
                                        SPDLOG_INFO("send Heart Beat to client");
                                        sendHeartbeat(vc->socket_fd, header.timePoint, CommunicationPhase::Response);
                                    }else{
                                        //响应数据已经准备好了
                                        it_response->second.lastActive = now;
                                        //数据已经准备好了，正在发送中
                                        if (header.type == ProtocolType::ReceiverACK){
                                            SPDLOG_INFO("accept ACK {} from message id {}", header.acceptOrder, header.timePoint);
                                            //发送数据
                                            it_response->second.setNewAckState(true);
                                            //收到了 ACK 确认
                                            it_response->second.setAckAccept(header.acceptOrder);
                                            //已经全部发送完毕，需要正常退出
                                            if (it_response->second.getAckAccept() == it_response->second.getPieceCount()){
                                                //发送完毕，正常断开链接 不需要删除，等待定时器删除
                                                SPDLOG_INFO("Sub Reactor finish {} request-response normally！", header.timePoint);
                                            }else{
                                                //没有发送完！
                                                sendResponseDataToClient(vc, it_response->second.getID());
                                            }
                                        }
                                        else if (header.type == ProtocolType::TimedOutResponseHeartbeat){
                                            //收到的心跳信息
                                            it_response->second.lastActive = now;//展示不知道有什么用
                                        }
                                        else if (header.type == ProtocolType::TimedOutRequestHeartbeat){
                                            //收到了请求心跳信息，有数据了 直接发数据，而不是心跳
                                            SPDLOG_INFO("accept TimedOutRequestHeartbeat from message id {}", header.acceptOrder, header.timePoint);
                                            if (it_response->second.getAckAccept() < it_response->second.getPieceCount()){
                                                sendResponseDataToClient(vc, it_response->second.getID());
                                            }else{
                                                SPDLOG_INFO("ACK {} P COUNT {} ", it_response->second.getAckAccept(), it_response->second.getPieceCount());
                                            }
                                        }
                                        else{
                                            //其他情况属于处理逻辑错误，不做响应
                                        }
                                    }
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
                }
                if (epollQueue[i].events & EPOLLOUT){
                    struct epoll_event ree{ EPOLLIN, {.fd = epoll_switch_fd} };
                    epoll_ctl(epollFd, EPOLL_CTL_MOD ,epoll_switch_fd, &ree);
                }
                if (epollQueue[i].events & EPOLLHUP){
                    SPDLOG_ERROR("epoll event EPOLLHUP Event errno: {}", errno);
                    throw std::runtime_error("exit"); //停掉服务直接退出
                }
                if (epollQueue[i].events & EPOLLRDHUP){
                    SPDLOG_ERROR("epoll event EPOLLRDHUP Event errno: {}", errno);
                    throw std::runtime_error("exit"); //停掉服务直接退出
                }
            }
        }
    }

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
            //是否是新数据
            bool is_new = std::get<4>(newConnections.front());

            bool isSuccess = false;
            auto header = protocol_util.parse(data.get(), _recv_length, isSuccess);

            if (isSuccess){
                VirtualConnection *vc = nullptr;
                if (is_new){
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

                    vc = connections + findResult.second;
                }
                else{
                    size_t idx = 0;
                    for (int k = 0; k < openMaxConnection; ++k) {
                        idx = (lastEmptyPlace + k) % openMaxConnection;
                        if (connections[idx].socket_fd == _socket_fd ){
                            vc = &connections[idx];
                            SPDLOG_ERROR("Sub-Reactor NO Throw data! Find socket_fd {}", _socket_fd);
                            break;
                        }
                        //只能在 [0 ~ openMaxConnection) 之间打转
                    }
                }
                //下一个直接丢弃
                if (vc == nullptr) {
                    SPDLOG_ERROR("Sub-Reactor Throw data!");
                    continue;
                };

                SPDLOG_INFO("Sub-Reactor parse data ok!");
                // 读取成功 上交报文，建立链接
                //协议类型
                Servlet servlet(header.timePoint, ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr));
                //看看当前报文存不存在
                auto it= messages.find(servlet);
                if (header.type == ProtocolType::RequestSend){
                    uint16_t  ACKNumber = 0; //确认号
                    message_queue::iterator ite;
                    if (it != messages.end()){
                        it->second.active_dt = GetNowTick();
                        //有历史报文
                        if(!it->second.getPieceState(header.pieceOrder)){
                            char * des = (char *)(it->second.data.get()) + (header.pieceStandardSize * header.pieceOrder);
                            // 完成报文内容拷贝
                            std::memcpy(des, data.get() + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
                            ACKNumber = it->second.setPieceState(header.pieceOrder, true);
                        }
                        ite = it;
                    }
                    else
                    {
                        //新的 message 报文
                        Request request(header.timePoint, ntohs(addr.sin_port), ntohl(addr.sin_addr.s_addr), header.totalCount, header.totalSize);
                        request.setSocket(_socket_fd);
                        ACKNumber = request.setPieceState(header.pieceOrder, true);
                        request.active_dt = GetNowTick();
                        auto store = pool->allocate(header.totalSize);
                        std::shared_ptr<char[]> dt((char*)store, [&, total=header.totalSize](char *ptr){
                            pool->deallocate(ptr, total);
                        });
                        request.data = dt;
                        char * des = (char *)store + (header.pieceStandardSize * header.pieceOrder);
                        // 完成报文内容拷贝
                        std::memcpy(des, data.get() + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
                        messages.emplace(servlet, request);
                        auto Pair = messages.emplace(servlet, request);
                        ite = Pair.first;
                        setRequestTimeOutEvent(servlet);
                    }

                    sendACK(_socket_fd, addr ,ACKNumber, header.timePoint);
                    if (ACKNumber == header.totalCount){ //最后一个了
                        vc->messages.emplace_back(header.timePoint); //存储ID
                        SPDLOG_INFO("Sub Reactor accept all data of the message_id {} {} {}", header.timePoint, _socket_fd, vc->messages.size());
                        if (!ite->second.getTriggerState()){
                            ite->second.trigger();
                            trigger(ite->second.getTotalDataSize(), ite->second.data ,ite->first, vc);
                        }
                    }
                }
                else if(header.type == ProtocolType::RequestACK){
                    if (it != messages.end()){
                        sendACK(_socket_fd, addr,it->second.getAckNumber(), header.timePoint);
                    }else{
                        SPDLOG_INFO("Sub-Reactor Send the StateReset successfully Protocol timePoint {}", header.timePoint);
                        sendReset(_socket_fd, header.timePoint, header.phase);
                    }
                }
                else{
                    if (it != messages.end()){
                        sendHeartbeat(_socket_fd, header.timePoint, header.phase);
                    }else{
                        SPDLOG_INFO("Sub-Reactor Send the StateReset successfully Protocol timePoint {}", header.timePoint);
                        sendReset(_socket_fd, header.timePoint, header.phase);
                    }
                }
                /* 处理完毕再加入 */
                if (is_new){
                    struct epoll_event ReactorEpollEvent{ EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP, {.ptr = vc} };

                    if( epoll_ctl(epollFd, EPOLL_CTL_ADD, _socket_fd, &ReactorEpollEvent) == - 1 ){
                        close(_socket_fd); //关闭 socket
                        SPDLOG_ERROR("Epoll Operation EPOLL_CTL_ADD failed in Sub-Reactor deal with new Connection errno {} !", errno);
                        continue;
                    }else{
                        SPDLOG_INFO("Sub-Reactor add socket {} to epoll loop!", _socket_fd);
                    }
                    //让链接断开的定时器
                    treeTimer.setTimeout(SubReactor::ConnectionTimeOut.count(), &SubReactor::checkDeleteSocket,this, vc);
                }
            }
            else{
                //协议格式不正确
                std::string message {"Please use the correct network protocol！\n"};
                sendto(_socket_fd, message.c_str() , sizeof(char)*message.size(),0, (struct sockaddr*)&addr, sizeof(addr));
            }
            newConnections.pop();
        }
    }

    SubReactor::~SubReactor() {
        runState.store(false, std::memory_order_release);
        //唤醒 epoll
        struct epoll_event ree{ EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP, {.fd = epoll_switch_fd} };
        epoll_ctl(epollFd, EPOLL_CTL_MOD ,epoll_switch_fd, &ree);
        //等待线程结束
        if (runner != nullptr){
            if (runner->joinable()) runner->join();
        }
        if (epollFd != -1) close(epollFd);
        delete [] connections;
        connections = nullptr;
        SPDLOG_INFO("Sub-Reactor end life!");
    }

    void SubReactor::checkDeleteSocket(VirtualConnection *vir){
        if (vir->socket_fd != -1 ){
            if ( GetNowTick() - vir->last_active > SubReactor::ConnectionTimeOut ){
                closeSocket(vir);
                vir->socket_fd = -1;
                vir->messages.clear();
                vir->responses.clear();
            }else{
                treeTimer.setTimeout(SubReactor::ConnectionTimeOut.count(), &SubReactor::checkDeleteSocket,this, vir);
            }
        }
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

    void SubReactor::sendACK(int _socket_fd, struct sockaddr_in addr, uint16_t _ack_number, uint64_t _message_id) {
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
        auto result = sendto(_socket_fd, ACKBuffer,muse::rpc::Protocol::protocolHeaderSize, 0, (struct sockaddr*)&addr, sizeof(addr));
        if (result == -1){
            SPDLOG_ERROR("Sub-Reactor Send the ACK {} failed Protocol timePoint {}" ,_ack_number, _message_id);
        }else{
            SPDLOG_INFO("Sub-Reactor Send the ACK {} successfully Protocol timePoint {}", _ack_number, _message_id);
        }
    }

    /* 标识当前链接已经被重置了 */
    void SubReactor::sendReset(int _socket_fd, uint64_t _message_id, CommunicationPhase phase) {
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
            SPDLOG_ERROR("Sub-Reactor Send the StateReset failed Protocol timePoint {} errno {}" ,_message_id, errno);
        }
    }

    /* 发送心跳信息 */
    void SubReactor::sendHeartbeat(int _socket_fd, uint64_t _message_id, CommunicationPhase phase) {
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
        protocol_util.setProtocolPhase(buffer, phase);
        if (result == -1){
            SPDLOG_ERROR("Sub-Reactor Send the Response Heart beat failed Protocol timePoint {}" ,_message_id);
        }
    }

    /* 发送服务器资源已经耗尽 */
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
        //看看还有没有空位置, 没有清楚一下
        if (activeConnectionsCount >= openMaxConnection){
            clearTimeoutConnections();
        }
        //从上一个非空位置就查找
        size_t idx = 0;
        for (int i = 0; i < openMaxConnection; ++i) {
            idx = (lastEmptyPlace + i) % openMaxConnection;
            if (connections[idx].socket_fd == -1 ){
                result.first = true;
                result.second = idx;
                //只能在 [0 ~ openMaxConnection) 之间打转
                lastEmptyPlace = idx;
                break;
            }
        }
        return result;
    }

    int SubReactor::clearTimeoutConnections() {
        int clearCount = 0;
        auto now = GetNowTick();
        for (int i = 0; i < openMaxConnection; ++i) {
            if (connections[i].socket_fd != -1 ){
                if (now -connections[i].last_active > SubReactor::ConnectionTimeOut){
                    closeSocket(&connections[i]);
                    connections[i].socket_fd = -1;
                    connections[i].messages.clear(); //清楚
                    connections[i].responses.clear(); //清楚
                    clearCount++;
                    this->activeConnectionsCount--;
                }
            }
        }
        return clearCount;
    }

    /* 不安全 迭代器很容易失效 生产任务 */
    void SubReactor::trigger(uint32_t data_size, std::shared_ptr<char[]> _full_data, Servlet servlet, VirtualConnection *vic){
        SPDLOG_INFO("Sub Reactor task has been commit to thread pool");
        auto ex = make_executor(&SubReactor::triggerTask, this, data_size,_full_data, servlet, vic);
        auto f = GetThreadPoolSingleton()->commit_executor(ex); //有可能会触发失败，因为任务队列已经满了
        if (!f.isSuccess){
            sendExhausted(vic->socket_fd); //服务器资源已经耗尽了
            SPDLOG_ERROR("Sub-Reactor kill request {} because thread pool fulled!", servlet.getID());
        }
    }

    /* 由线程池来运行、需要加锁 */
    void SubReactor::triggerTask(uint32_t data_size, std::shared_ptr<char[]> _full_data,  Servlet servlet, VirtualConnection *vc) {
        //管道内部的数据可能有问题！！！
        auto data_tuple = MiddlewareChannel::GetInstance()->ClientIn(std::move(_full_data), data_size , pool);
        std::shared_ptr<char[]> pure_data = std::get<0>(data_tuple);
        auto pure_data_count = std::get<1>(data_tuple);
        muse::serializer::BinaryDeserializeView view(pure_data.get(), pure_data_count);
        std::string request_rpc_name;
        //获得方法名称
        view.output(request_rpc_name);
        auto router = MiddlewareChannel::GetInstance()->services.rbegin();
        //获得 RouteService 实列
        if (request_rpc_name.find(RpcResponseHeader::prefix_name) == 0){
            uint32_t client_ip_address = vc->addr.sin_addr.s_addr;
            uint16_t client_port = vc->addr.sin_port;

            muse::serializer::BinarySerializer serializer_ip_port;
            bool no_other_args = (pure_data_count == view.getReadPosition());
            if (no_other_args){
                serializer_ip_port.input(std::make_tuple(client_ip_address,client_port));
            }else{
                serializer_ip_port.inputArgs(client_ip_address, client_port);
            }
            auto new_pure_data_count = pure_data_count + serializer_ip_port.byteCount();

            auto store = pool->allocate(new_pure_data_count);
            std::shared_ptr<char[]> dt((char*)store, [=](char *ptr){
                pool->deallocate(ptr, new_pure_data_count);
            });
            std::memcpy(store, pure_data.get(), pure_data_count);
            std::memcpy((char*)store + pure_data_count, serializer_ip_port.getBinaryStream(), serializer_ip_port.byteCount());
            //data_tuple = (*router)->In(dt, new_pure_data_count, pool);

            //有其他参数需要修改长度
            if (!no_other_args){
                auto pos = view.getReadPosition() + sizeof(muse::serializer::BinaryDataType);
                auto char_pos = (char*)dt.get() + pos;

                muse::serializer::BinarySerializer::Tuple_Element_Length
                tpl_size = *reinterpret_cast<uint16_t *>(char_pos); //元祖长度
                //如果当前机器是大端序
                // 小端序 ---> 大端序
                if (sq == muse::serializer::ByteSequence::BigEndian){
                    auto first = (char*)&tpl_size;
                    auto last = first + sizeof(muse::serializer::BinarySerializer::Tuple_Element_Length);
                    std::reverse(first, last);
                }
                tpl_size += 2; // 把数组长度  + 2
                //如果是大端序，数据插回去
                // 大端序 ---> 小端序
                if (sq == muse::serializer::ByteSequence::BigEndian){
                    auto first = (char*)&tpl_size;
                    auto last = first + sizeof(muse::serializer::BinarySerializer::Tuple_Element_Length);
                    std::reverse(first, last);
                }
                //覆盖原来的数组长度
                std::memcpy(char_pos, (char*)&tpl_size, sizeof(muse::serializer::BinarySerializer::Tuple_Element_Length));
            }
            data_tuple = (*router)->In(dt, new_pure_data_count, pool);
        }else{
            data_tuple = (*router)->In(pure_data, pure_data_count, pool);
        }

        //获得解压的数据
        auto send_data_tuple = MiddlewareChannel::GetInstance()->Out(std::get<0>(data_tuple),std::get<1>(data_tuple),pool);

        std::shared_ptr<char[]> send = std::get<0>(send_data_tuple);
        auto count = std::get<1>(send_data_tuple);
        char * data = (char*)pool->allocate(count);
        std::memcpy(data, send.get(), count);
        //设置一个超时时间，将数据发送给客户端
        try{
            std::lock_guard<std::mutex> lock(treeTimer_mtx);
            treeTimer.setTimeout(5,[this](char* dt, VirtualConnection *vc, Servlet _servlet, uint32_t count){
                uint16_t _pieces = count/Protocol::defaultPieceLimitSize + (((count % Protocol::defaultPieceLimitSize) == 0)?0:1);
                Response response(_servlet.getID(),_servlet.getHostPort(), _servlet.getIpAddress(),_pieces, count,dt);
                response.pool = pool;
                auto result = vc->responses.insert_or_assign( _servlet.getID(), response);
                if (!result.second){
                    SPDLOG_INFO("The response data add failed ");
                }else{
                    SPDLOG_INFO("The response data has been transferred to the response object");
                    sendResponseDataToClient(vc, _servlet.getID());
                }
                setResponseClearTimeout(vc, _servlet.getID());
            },data, vc,servlet,count);

        } catch (const std::exception &ex) {
            SPDLOG_ERROR("Sub-Reactor setTimeout failed!");
        }
    }
    /* 关闭 */
    void SubReactor::stop() {
        // 唤醒 epoll
        struct epoll_event ree{ EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP, {.fd = epoll_switch_fd} };
        epoll_ctl(epollFd, EPOLL_CTL_MOD ,epoll_switch_fd, &ree);
        // 发起信号
        runState.store(false,std::memory_order_relaxed);
        //停止所有的线程
        if (runner != nullptr && runner->joinable()){
            runner->join();
        }
        /* 关闭所有的 socket */
        for (int i = 0; i < openMaxConnection; ++i) {
            close(connections[i].socket_fd);
        }
        /* 清楚所有的消息 */
        messages.clear();
        //关闭 epoll
        if (epollFd != -1) close(epollFd);
    }

    void SubReactor::closeSocket(VirtualConnection *vc) const {
        sockaddr_in addrClose{};
        socklen_t clen = sizeof(addrClose);;
        addrClose.sin_family = AF_UNSPEC;
        try {
            auto result = connect(vc->socket_fd, (struct sockaddr*)&addrClose, clen );
            epoll_ctl(epollFd, EPOLL_CTL_DEL, vc->socket_fd,nullptr);
            auto close_result = close(vc->socket_fd);
            if (result == -1 || close_result == -1){
                SPDLOG_ERROR("Close socket Error {}", vc->socket_fd);
            }
            {
                std::lock_guard<std::mutex> lock(GlobalEntry::mtx);
                GlobalEntry::timerTree->setTimeout(0, [](uint16_t port, uint32_t ip){
                    Peer pr {ntohs(port), ntohl(ip) };
                    if (GlobalEntry::con_queue->count(pr) == 1){
                        GlobalEntry::con_queue->erase(pr);
                    }else{
                    }
                }, vc->addr.sin_port, vc->addr.sin_addr.s_addr);
            }
        }catch(const std::exception& ex){
            SPDLOG_ERROR("Close socket {} throw exception what() ", vc->socket_fd, ex.what());
        }
    }

    void SubReactor::sendRequireACK(int _socket_fd, uint64_t _message_id, uint16_t _accept_min_order, CommunicationPhase phase) {
        char buffer[muse::rpc::Protocol::protocolHeaderSize];
        protocol_util.initiateSenderProtocolHeader(buffer,_message_id,0,Protocol::protocolHeaderSize);
        protocol_util.setACKAcceptOrder(buffer, _accept_min_order);
        protocol_util.setProtocolType(buffer,ProtocolType::RequestACK);
        protocol_util.setProtocolPhase(buffer, phase); //二阶段消息
        auto result = ::write(_socket_fd, buffer,  Protocol::protocolHeaderSize);
        if (result == -1){
            SPDLOG_ERROR("Sub-Reactor sendRequireACK failed Protocol timePoint {}", _message_id);
        }
    }

    void SubReactor::judgeDelete(const Servlet& _servlet){
        auto it= messages.find(_servlet);
        if (it != messages.end()){
            if ( GetNowTick() - it->second.active_dt > SubReactor::RequestTimeOut){
                messages.erase(_servlet);
                SPDLOG_INFO("Sub-Reactor kill request timeout timer for {}", _servlet.getID());
            }else{
                setRequestTimeOutEvent(_servlet);
            }
        }
    }

    void SubReactor::setRequestTimeOutEvent(const Servlet& _servlet) {
        //注册超时时间，清楚 request
        treeTimer.setTimeout(SubReactor::RequestTimeOut.count(),&SubReactor::judgeDelete, this, _servlet);
    }

    void SubReactor::sendResponseDataToClient(VirtualConnection *vir, uint64_t _message_id) {
        try{
            char sendBuf_[Protocol::FullPieceSize + 1] = { '\0' };
            auto res = vir->responses.find(_message_id);
            if (vir->responses.count(_message_id) <= 0){
                return;
            }
            if (res->second.getAckAccept() == res->second.getPieceCount()){
                //已经发完了
                return;
            }
            //发送次数
            auto times = 1;
            auto ack_accept = res->second.getAckAccept();
            auto piece_count = res->second.getPieceCount();
            auto last_piece_size = res->second.getLastPieceSize();
            uint16_t start = ack_accept + times;
            for (uint16_t k = ack_accept; k < start && k < piece_count; ++k) {
                protocol_util.initiateSenderProtocolHeader(sendBuf_,res->second.getID(),res->second.getTotalDataSize(), Protocol::protocolHeaderSize);
                protocol_util.setProtocolType(sendBuf_,ProtocolType::RequestSend);      //设置报文类型
                protocol_util.setProtocolPhase(sendBuf_,CommunicationPhase::Response); //设置为二阶段
                uint16_t pieceDataPartSize = (k == (piece_count - 1))?last_piece_size: Protocol::defaultPieceLimitSize;
                protocol_util.setProtocolOrderAndSize(sendBuf_, k, pieceDataPartSize);
                std::memcpy(sendBuf_ + Protocol::protocolHeaderSize, res->second.data + (k * Protocol::defaultPieceLimitSize), pieceDataPartSize); //body
                ::sendto(vir->socket_fd, sendBuf_,  Protocol::protocolHeaderSize + pieceDataPartSize, 0,(struct sockaddr*)&(vir->addr), sizeof(vir->addr));
            }
        } catch (const std::exception &ex) {
            SPDLOG_ERROR("SubReactor sendResponseDataToClient error {}", ex.what());
        }
    }

    void SubReactor::setResponseClearTimeout(VirtualConnection *vir, uint64_t _message_id) {
        treeTimer.setTimeout(SubReactor::ResponseTimeOut.count(),&SubReactor::judgeResponseClearTimeout, this, vir, _message_id);
    }

    void SubReactor::judgeResponseClearTimeout(VirtualConnection *vir, uint64_t _message_id) {
        if (vir->socket_fd != -1){
            if (vir->responses.count(_message_id) > 0){
                auto  it = vir->responses.find(_message_id);
                if (GetNowTick() -  it->second.lastActive > SubReactor::ResponseTimeOut){
                    vir->messages.remove(_message_id); //消息ID 也删除了
                    vir->responses.erase(_message_id);        //删除数据
                    SPDLOG_INFO("Sub-Reactor kill response timeout timer for {}",_message_id );
                }else{
                    setResponseClearTimeout(vir, _message_id);
                }
            }
        }
    }
}
