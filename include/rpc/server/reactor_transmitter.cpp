//
// Created by remix on 23-8-20.
//

#include "reactor_transmitter.hpp"
namespace muse::rpc{
    ReactorTransmitter::~ReactorTransmitter() {
        run_state.store(false, std::memory_order_relaxed);
        //等待线程死掉
        if (runner != nullptr){
            if (runner->joinable()) runner->join();
            SPDLOG_INFO("ReactorTransmitter.Runner die!");
        }
    }

    //开启迭代器
    void ReactorTransmitter::loop() {
        //创建服务器 socket
        if( (socket_fd = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK,IPPROTO_UDP)) == -1 ){
            SPDLOG_ERROR("Main-Reactor loop function socket fd create failed, errno {}", port, errno);
            throw ReactorException("[Main-Reactor]", ReactorError::SocketFdCreateFailed);
        }
        //服务器地址
        sockaddr_in serverAddress_{
                PF_INET,
                htons(port),
                htonl(INADDR_ANY)
        };

        struct timeval tm = {recv_gap.count() / 1000, (recv_gap.count() * 1000) % 1000000 };
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm, sizeof(struct timeval));
        //获得内存池

        int on = 1;
        //地址复用 端口复用
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        //地址复用 端口复用
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

        //绑定端口号 和 IP
        if(bind(socket_fd, (const struct sockaddr*)&serverAddress_, sizeof(serverAddress_)) == -1){
            SPDLOG_ERROR("socket bind port {} error, errno {}", port, errno);
            throw ReactorException("[Main-Reactor]", ReactorError::SocketBindFailed);
        }
        //启动
        run_state.store(true, std::memory_order_release);
        //启动从引擎
        startSubReactor(); //失败直接抛出异常
        while (run_state.load(std::memory_order::memory_order_release)){
            // 持有锁,处理新到的链接
            {
                std::unique_lock<std::mutex> lock(new_messages_mtx);
                if (!new_messages.empty()){
                    // 插入到任务重
                    for (auto &it: new_messages) {
                        if (messages.count(it.first) == 0){
                            messages[it.first] = it.second;
                            SPDLOG_INFO("ReactorTransmitter add message {} ", it.second->message_id);
                        }
                    }
                    //清楚所有的信息
                    new_messages.clear();
                }

            }
            // 执行发送任务,只有消息处于 request 阶段 才会发送
            for (auto &message:  messages) {
                //属于头次发送
                Servlet servlet(message.second->message_id, ntohs(message.second->server_address.sin_port), ntohl(message.second->server_address.sin_addr.s_addr));
                if (message.second->last_active == 0ms){
                    send_data(message.second); //发送数据给服务器那边
                    message.second->last_active = GetTick();     // 更新活跃时间
                    //设置一个超时时间
                    timer.setTimeout(this->request_timeout.count(),&ReactorTransmitter::timeout_event,this,servlet, message.second->ack_accept);
                }else{
                    //数据还处于 1 阶段、表示数据还没有发送完毕
                    if (message.second->phase == CommunicationPhase::Request){
                        //消息还没有进入删除阶段
                        if (message.second->timeout_time < try_time){
                            // 判断是否收到回信、收到就发送
                            if(message.second->ack_accept > message.second->ack_expect - message.second->send_time){
                                send_data(message.second);
                                //发送后设置超时定时器
                                timer.setTimeout(this->request_timeout.count(),&ReactorTransmitter::timeout_event,this,servlet,message.second->ack_accept);
                            }
                        }
                    }
                }
            }
            // 等待响应，这个阶段可以进行移除message的事宜
            ssize_t recvLen;
            struct sockaddr_in sender_address{};
            socklen_t len = 0;
            //一直读，一直到为空
            do{
                recvLen = ::recvfrom(socket_fd, read_buffer, Protocol::FullPieceSize + 1,0, (struct sockaddr*)&sender_address, &len);
                if (recvLen >= muse::rpc::Protocol::protocolHeaderSize){
                    bool isSuccess = false;
                    auto header = protocol.parse(read_buffer, recvLen, isSuccess);
                    if (!isSuccess){
                        //解析失败、协议格式不正确
                        std::string message {"Please use the correct network protocol！\n"};
                        sendto(socket_fd, message.c_str() , sizeof(char)*message.size(),0, (struct sockaddr*)&sender_address, sizeof(sender_address));
                        continue; //直接下一个
                    }
                    //先判断消息是否是自己需要的回信
                    Servlet servlet(header.timePoint, ntohs(sender_address.sin_port), ntohl(sender_address.sin_addr.s_addr));
                    auto msg = messages.find(servlet);
                    if (msg == messages.end()){
                        //已经创建连接了
                        void * dp = pool->allocate( Protocol::FullPieceSize + 1);
                        std::shared_ptr<char[]> dt((char*)dp, [&](char *ptr){
                            pool->deallocate(ptr, Protocol::FullPieceSize + 1);
                        });
                        ::memcpy(dt.get(), read_buffer, recvLen);
                        //解析成功，创建 一个二元组
                        Peer peer(ntohs(sender_address.sin_port), ntohl(sender_address.sin_addr.s_addr));
                        //查找
                        auto it= GlobalEntry::con_queue->find(peer);
                        if (it != GlobalEntry::con_queue->end()){
                            subs[it->second.SubReactor_index]->acceptConnection(it->second.socket, recvLen, sender_address, dt, false);
                            SPDLOG_INFO("directly send data to sub reactor because vir connection is build!");
                            continue; //直接下一个
                        }else{
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
                                if(bind(sonSocketFd, (const struct sockaddr*)&serverAddress_, sizeof(serverAddress_)) == -1){
                                    SPDLOG_ERROR("son socket bind port {} failed, errno {}", port, errno);
                                }
                                // addr 是 客户端地址
                                if (connect(sonSocketFd, (struct sockaddr *) &sender_address, sizeof(struct sockaddr)) == -1) {
                                    SPDLOG_ERROR("son socket connect ip {} port {} failed, errno {}",inet_ntoa(sender_address.sin_addr), ntohs(port), errno);
                                } else {
                                    //成功链接，丢给 从反应堆
                                    uint32_t idx = counter % subReactorCount;
                                    counter++;
                                    SPDLOG_INFO("Main-Reactor send connection to {} sub reactor",idx);
                                    auto result = subs[idx]->acceptConnection(sonSocketFd, recvLen, sender_address, dt, true);
                                    if (!result){
                                        SPDLOG_WARN("Sub Reactor has been stopped!");
                                        close(sonSocketFd);
                                    }else{
                                        GlobalEntry::con_queue->insert_or_assign(peer, SocketConnection{sonSocketFd, idx} );
                                    }
                                }
                            }
                        }
                    }else{
                        if (msg == messages.end()) continue;  // 没有历史消息
                        msg->second->last_active = GetTick(); // 更新活跃时间
                        msg->second->timeout_time = 0;
                        if (header.phase == CommunicationPhase::Request)
                        {
                            //收到确认ACK
                            if (header.type == ProtocolType::ReceiverACK){
                                //SPDLOG_INFO("ReactorTransmitter get ACK {} of {}!" , header.acceptOrder ,msg->second->message_id);
                                if (header.acceptOrder > msg->second->ack_accept){
                                    msg->second->ack_accept = header.acceptOrder;
                                    //对方已经收到所有的数据
                                    if (header.acceptOrder == msg->second->piece_count){
                                        //最后一个了
                                        //SPDLOG_INFO("Sub Reactor send all data of the message_id {}", header.timePoint);
                                        //转变阶段
                                        msg->second->phase = CommunicationPhase::Response;
                                        //二阶段超时心跳
                                        response_timeout_event(msg->first,  msg->second->last_active);
                                    }
                                }
                            }// 对面没有收到前面的消息，但是你发了 RequestACK
                            else if (header.type == ProtocolType::StateReset){
                                //需要重新发送数据
                                msg->second->timeout_time = 0;
                                msg->second->ack_accept = 0;
                                send_data(msg->second);
                                timer.setTimeout(this->request_timeout.count(), &ReactorTransmitter::timeout_event, this,  servlet,  msg->second->ack_accept);
                            }
                                // 服务器资源已经耗尽，无法再处理下去
                            else if (header.type == ProtocolType::TheServerResourcesExhausted){
                                msg->second->response_data.isSuccess = false;
                                msg->second->response_data.reason = FailureReason::TheServerResourcesExhausted;
                                try_trigger(msg->second);
                            }
                                // 服务器返回心跳信息
                            else if (header.type == ProtocolType::TimedOutResponseHeartbeat){
                                //msg->second->timeout_time = 0; //已经统一执行了
                                //SPDLOG_INFO("ReactorTransmitter get ResponseHeartbeat from server!" , header.acceptOrder ,msg->second->message_id);
                            }
                                // 服务器请求心跳信息
                            else if (header.type == ProtocolType::TimedOutRequestHeartbeat){
                                send_heart_beat(msg->second, header.phase);
                            }
                        }
                        else if(header.phase == CommunicationPhase::Response){
                            //网络并不会按序到达
                            if (msg->second->phase == CommunicationPhase::Request){
                                msg->second->phase = CommunicationPhase::Response;
                                msg->second->ack_accept = msg->second->piece_count;
                            }
                                // 收到响应数据
                            else if (header.type == ProtocolType::RequestSend){
                                //SPDLOG_INFO("ReactorTransmitter get data order {} of {} from server!" , header.pieceOrder ,msg->second->message_id);
                                //收到输出，对响应对象初始化
                                msg->second->response_data.initialize(header.timePoint, header.totalCount, header.totalSize);
                                if (!msg->second->response_data.getPieceState(header.pieceOrder)){
                                    auto des = msg->second->response_data.data.get() +  header.pieceOrder * Protocol::defaultPieceLimitSize;
                                    // 完成报文内容拷贝
                                    std::memcpy(des, read_buffer + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
                                }
                                auto ACK = msg->second->response_data.setPieceState(header.pieceOrder , true);
                                //发送ACK
                                send_response_ACK(msg->second, ACK);
                                //收到所有的数据
                                if (ACK == msg->second->response_data.piece_count){
                                    msg->second->response_data.isSuccess = true;
                                    //SPDLOG_INFO(" try trigger get all data!");
                                    try_trigger(msg->second);
                                }else{
                                    //开启一个定时器等待后面的数据
                                    response_timeout_event(msg->first,  msg->second->last_active);
                                }
                            }
                                // 服务端请求 ACK  [ 暂未纳入协议流程 ]
                            else if(header.type == ProtocolType::RequestACK){
                                auto ACK = msg->second->response_data.setPieceState(header.pieceOrder , true);
                                //发送ACK
                                send_response_ACK(msg->second, ACK);
                            }
                                //收到心跳数据，服务器正在处理数据
                            else if(header.type == ProtocolType::TimedOutResponseHeartbeat){
                                //再等待一个超时时间
                                response_timeout_event(msg->first,  msg->second->last_active);
                                //SPDLOG_INFO("ReactorTransmitter get ResponseHeartbeat from server!" , header.acceptOrder ,msg->second->message_id);
                            }
                                // 服务器请求心跳信息 [ 暂未纳入协议流程 ]
                            else if (header.type == ProtocolType::TimedOutRequestHeartbeat){
                                send_heart_beat(msg->second, CommunicationPhase::Response);
                            }
                            //其他情况不作回应
                        }
                    }
                }
            }
            while (recvLen  > 0);
            //处理定时器时间
            {
                std::lock_guard<std::mutex> lock(timer_mtx);
                timer.runTask();
            }
            //处理删除 socket 事件
            {
                std::lock_guard<std::mutex> lock(GlobalEntry::mtx);
                GlobalEntry::timerTree->runTask();
            }

        }
        //循环读 直到读不到任何数据
    }

    bool ReactorTransmitter::send(TransmitterEvent&& event) {
        //判断是否是多次发送同一个 event
        if (event.get_callBack_state() && event.get_remote_state()){
            //创建一个任务
            std::shared_ptr<TransmitterTask> taskPtr = std::make_shared<TransmitterTask>(std::move(event), GlobalMicrosecondsId());
            // 设置数据
            std::shared_ptr<char[]> data_share(const_cast<char*>(taskPtr->event.get_serializer().getBinaryStream()), [](char *ptr){});
            // 中间件通道处理
            auto tpl =  MiddlewareChannel::GetInstance()->ClientOut(data_share, taskPtr->event.get_serializer().byteCount(), pool);

            taskPtr->data = std::get<0>(tpl);           // 数据
            taskPtr->total_size = std::get<1>(tpl);     // 总数据
            taskPtr->ack_accept = 0;                       // 已经收到的ack
            taskPtr->phase = CommunicationPhase::Request;  // 通信阶段
            // 总分片数量
            taskPtr->piece_count =  taskPtr->total_size / Protocol::defaultPieceLimitSize + (((taskPtr->total_size % Protocol::defaultPieceLimitSize) == 0)?0:1);
            taskPtr->message_id = taskPtr->message_id;
            // 获得发送次数
            taskPtr->send_time = Protocol::getSendCount(taskPtr->piece_count);
            taskPtr->last_piece_size = (taskPtr->total_size % Protocol::defaultPieceLimitSize == 0)? Protocol::defaultPieceLimitSize:taskPtr->total_size % Protocol::defaultPieceLimitSize ; /* 数据部分 */
            {
                std::unique_lock<std::mutex> lock(new_messages_mtx);
                Servlet base(taskPtr->message_id, ntohs(taskPtr->server_address.sin_port), ntohl(taskPtr->server_address.sin_addr.s_addr));
                // 插入到任务队列中
                if (messages.count(base) == 0)
                {
                    messages[base] =  taskPtr;
                    return true;
                }
            }
        }
        return false;
    }

    void ReactorTransmitter::send_data(const std::shared_ptr<TransmitterTask>& task) {
        uint16_t end = task->ack_accept + task->send_time;
        uint16_t i = task->ack_accept;
        for (; i < end && i < task->piece_count; ++i) {
            protocol.initiateSenderProtocolHeader(buffer,task->message_id,task->total_size,Protocol::protocolHeaderSize);
            protocol.setProtocolType(buffer,ProtocolType::RequestSend);
            // 计算当前的 piece 的大小
            uint16_t pieceDataPartSize = (i == (task->piece_count - 1))?task->last_piece_size: Protocol::defaultPieceLimitSize;
            // 设置序号和大小
            protocol.setProtocolOrderAndSize(buffer, i, pieceDataPartSize);
            std::memcpy(buffer + Protocol::protocolHeaderSize, task->data.get() + (i * Protocol::defaultPieceLimitSize), pieceDataPartSize); //body
            // 发送输出到服务器
            ::sendto(socket_fd, buffer,  Protocol::protocolHeaderSize + pieceDataPartSize, 0, (struct sockaddr*)&(task->server_address), sizeof(task->server_address));
            SPDLOG_INFO("ReactorTransmitter send request data {} order {}", pieceDataPartSize, i);
        }
        task->ack_expect = i; //预期 ack_expect
    }

    void ReactorTransmitter::set_request_timeout(const uint32_t &_timeout) {
        this->request_timeout = std::chrono::milliseconds(_timeout);
    }

    void ReactorTransmitter::set_response_timeout(const uint32_t &_timeout) {
        this->response_timeout = std::chrono::milliseconds(_timeout);
    }

    void ReactorTransmitter::set_try_time(const uint16_t &_try) {
        this->try_time = _try;
    }

    void ReactorTransmitter::request_ACK(const std::shared_ptr<TransmitterTask>& task){
        //超时没有收到消息
        protocol.initiateSenderProtocolHeader(buffer,
              task->message_id,
              task->total_size,
              Protocol::protocolHeaderSize
        );
        protocol.setProtocolType(buffer,ProtocolType::RequestACK);
        ::sendto(socket_fd,
                 buffer,
                 Protocol::protocolHeaderSize,0,
                 (struct sockaddr*)&(task->server_address),
                 sizeof(task->server_address)
        );
    }

    void ReactorTransmitter::trigger(Servlet servlet){
        auto it = messages.find(servlet);
        if (it != messages.end()){
            //执行回调函数
            if (it->second->response_data.isSuccess){
                //正确获得了所有的数据
                auto realTpl = MiddlewareChannel::GetInstance()->ClientIn(it->second->response_data.data, it->second->response_data.total_size, pool);
                it->second->response_data.data = std::get<0>(realTpl);
                it->second->response_data.total_size = std::get<1>(realTpl);
            }
            it->second->event.trigger_callBack(it->second->response_data);
            //设置一个定时器，然后删除当前消息
            {
                std::lock_guard<std::mutex> lock(timer_mtx);
                timer.setTimeout(0, &ReactorTransmitter::delete_message, this, servlet);
            }
        }
    }

    void ReactorTransmitter::try_trigger(const std::shared_ptr<TransmitterTask>& msg){
        if (!msg->is_trigger){
            msg->is_trigger = true;
            Servlet base(msg->message_id, ntohs(msg->server_address.sin_port), ntohl(msg->server_address.sin_addr.s_addr));
            if(workers != nullptr){
                auto ex = make_executor(&ReactorTransmitter::trigger, this, base);
                auto result = workers->commit_executor(ex);
                if (!result.isSuccess){
                    //如果提交失败咋办？
                    SPDLOG_ERROR("ReactorTransmitter::trigger commit to thread pool error!", msg->message_id);
                    //开线程
                    auto fu = std::async(std::launch::async,&ReactorTransmitter::trigger, this, base);
                }
            }else{
                trigger(base);
            }
        }
    }
    void ReactorTransmitter::response_timeout_event(Servlet servlet, std::chrono::milliseconds last_active) {
        auto it = messages.find(servlet);
        if (it != messages.end()){
            //没有收到新的消息
            if (it->second->last_active == last_active){
                it->second->timeout_time++;
                if (it->second->timeout_time > try_time){
                    //三次超时了,还没有触发，也就是还没有收到所有的数据
                    if (!it->second->is_trigger){
                        //触发
                        it->second->response_data.set_success(false);
                        it->second->response_data.reason = FailureReason::NetworkTimeout;
                        try_trigger(it->second); //触发
                    }
                }else{
                    send_request_heart_beat(it->second, CommunicationPhase::Response);
                    //再设置一个定时器
                    timer.setTimeout(this->request_timeout.count(), &ReactorTransmitter::response_timeout_event, this,  servlet, last_active);
                }
            }
        }
    }

    void ReactorTransmitter::timeout_event(Servlet servlet, uint64_t cur_ack) {
        auto it = messages.find(servlet);
        if (it != messages.end()){
            //检测ack
            if (it->second->ack_accept == cur_ack && it->second->ack_accept != it->second->piece_count ){
                //真超时了
                it->second->timeout_time++;
                //需要丢弃 报文 请求已经无法推进
                if (it->second->timeout_time > try_time){
                    //丢弃
                    it->second->response_data.set_success(false);
                    it->second->response_data.reason = FailureReason::NetworkTimeout;
                    //触发,把任务提交给线程池
                    try_trigger(it->second);
                }else{
                    //发送请求报文
                    request_ACK(it->second);
                    //SPDLOG_ERROR("ReactorTransmitter::trigger timout {}", it->second->message_id);
                    //再设置一个定时器
                    timer.setTimeout(this->request_timeout.count(), &ReactorTransmitter::timeout_event, this,  servlet, cur_ack);
                }
            }
        }
    }

    void ReactorTransmitter::delete_message(Servlet servlet) {
        if (messages.count(servlet)){
            SPDLOG_INFO("ReactorTransmitter delete message {} !", servlet.getID());
            messages.erase(servlet);
        }
    }

    void ReactorTransmitter::send_response_ACK(const std::shared_ptr<TransmitterTask>& task, const uint16_t &_ack_number) {
        protocol.initiateSenderProtocolHeader(
                buffer,
                task->message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol.setProtocolType(
                buffer,
                ProtocolType::ReceiverACK
        );
        protocol.setProtocolPhase(buffer,CommunicationPhase::Response);
        protocol.setACKAcceptOrder(buffer, _ack_number);
        ::sendto(
                socket_fd,
                buffer,
                Protocol::protocolHeaderSize,
                0,
                (struct sockaddr*)&(task->server_address),
                sizeof(task->server_address)
        );
    }

    //发送心跳信息
    void ReactorTransmitter::send_heart_beat(const std::shared_ptr<TransmitterTask>& task, CommunicationPhase phase) {
        protocol.initiateSenderProtocolHeader(
                buffer,
                task->message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol.setProtocolType(
                buffer,
                ProtocolType::TimedOutResponseHeartbeat
        );
        protocol.setProtocolPhase(buffer,phase);
        ::sendto(socket_fd, buffer,  Protocol::protocolHeaderSize, 0, (struct sockaddr*)&(task->server_address), sizeof(task->server_address));
    }

    void ReactorTransmitter::send_request_heart_beat(const std::shared_ptr<TransmitterTask>& task, CommunicationPhase phase) {
        protocol.initiateSenderProtocolHeader(
                buffer,
                task->message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol.setProtocolType(
                buffer,
                ProtocolType::TimedOutRequestHeartbeat
        );
        protocol.setProtocolPhase(buffer,phase);
        ::sendto(socket_fd, buffer,  Protocol::protocolHeaderSize, 0, (struct sockaddr*)&(task->server_address), sizeof(task->server_address));
    }

    void ReactorTransmitter::stop() noexcept {
        run_state.store(false, std::memory_order_release);
        if (runner != nullptr){
            //关闭主反应堆
            if (runner->joinable()) runner->join();
        }
        //关闭从反应堆
        for (auto &sub: subs ) {
            sub->stop();
        }
    }

    ReactorTransmitter::ReactorTransmitter(uint16_t _port, uint32_t _sub_reactor_count, uint32_t _open_max_connection, ReactorRuntimeThread _type):
    port(_port),
    type(_type),
    openMaxConnection(_open_max_connection),
    subReactorCount(_sub_reactor_count),
    runner(nullptr),
    counter(0), //负载均衡策略
    socket_fd(-1){
        pool = MemoryPoolSingleton();
    }

    void ReactorTransmitter::startSubReactor() {
        for (int i = 0; i < subReactorCount; ++i) {
            if (subs[i] != nullptr){
                subs[i]->start();
            }
        }
    }

    void ReactorTransmitter::start() {
        run_state.store(true, std::memory_order_relaxed);
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
                //启动自身
                runner = std::make_shared<std::thread>(&ReactorTransmitter::loop, this);
            }catch(std::exception &exp){
                throw ReactorException("[Main-Reactor]", ReactorError::CreateMainReactorThreadFailed);
            }
        }
    }
}