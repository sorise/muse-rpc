//
// Created by remix on 23-8-12.
//

#include "transmitter.hpp"

namespace muse::rpc {

    TransmitterTask::TransmitterTask(TransmitterEvent && _event, const uint64_t& _message_id)
    :event(std::move(_event)),
     phase(CommunicationPhase::Request), message_id(_message_id),data(nullptr),response_data()
    {
        if(1 != inet_aton(event.get_ip_address().c_str(),&server_address.sin_addr)){
            throw ClientException("ip address not right", ClientError::IPAddressError);
        }
        server_address.sin_family = PF_INET;
        server_address.sin_port = htons(event.get_port());
    }

    Transmitter::Transmitter(const uint16_t& _port, const std::shared_ptr<ThreadPool>& _workers):
    workers(_workers),
    port(_port)
    {
        this->socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
        if(this->socket_fd == -1){
            throw std::runtime_error("socket create failed");
        }
        sockaddr_in localAddress{
                PF_INET,
                htons(_port),
                htonl(INADDR_ANY)
        };

        if(::bind(this->socket_fd, (const struct sockaddr *)&localAddress, sizeof(localAddress)) == -1){
            throw std::runtime_error("socket bind failed, please check the port! ");
        }
        struct timeval tm = {recv_gap.count() / 1000, (recv_gap.count() * 1000) % 1000000 };
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm, sizeof(struct timeval));
        //获得内存池
        pool = MemoryPoolSingleton();
    }

    Transmitter::~Transmitter() {
        run_state.store(false, std::memory_order_relaxed);
        condition.notify_one();
        //等待线程死掉
        if (runner != nullptr){
            if (runner->joinable()) runner->join();
            //SPDLOG_INFO("Runner die!");
        }
    }

    //开启迭代器
    void Transmitter::loop() {
        while (run_state.load(std::memory_order::memory_order_relaxed)){
            /* 阻塞当前工作线程，防止CPU空转 */
            std::unique_lock<std::mutex> uniqueLock(condition_mtx);
            condition.wait(uniqueLock, [this]{
                if(!run_state.load(std::memory_order::memory_order_relaxed))
                    return true;
                return !new_messages.empty() ||  !messages.empty();
            });
            // 持有锁,处理新到的链接
            {
                std::unique_lock<std::mutex> lock(new_messages_mtx);
                if (!new_messages.empty()){
                    // 插入到任务重
                    for (auto &it: new_messages) {
                        if (messages.count(it.first) == 0){
                            messages[it.first] = it.second;
                            SPDLOG_INFO("Transmitter add message {} ", it.second->message_id);
                        }
                    }
                    //清楚所有的信息
                    new_messages.clear();
                }
            }
            // 执行发送任务,只有消息处于 request 阶段 才会发送
            for (auto &message:  messages) {
                //属于头次发送
                if (message.second->last_active == 0ms){
                    send_data(message.second); //发送数据给服务器那边
                    message.second->last_active = GetTick();     // 更新活跃时间
                    //设置一个超时时间
                    timer.setTimeout(this->request_timeout.count(),&Transmitter::timeout_event,this,message.second->message_id,message.second->ack_accept);
                }else{
                    //数据还处于 1 阶段、表示数据还没有发送完毕
                    if (message.second->phase == CommunicationPhase::Request){
                        //消息还没有进入删除阶段
                        if (message.second->timeout_time < try_time){
                            // 判断是否收到回信、收到就发送
                            if(message.second->ack_accept > message.second->ack_expect - message.second->send_time){
                                send_data(message.second);
                                //发送后设置超时定时器
                                timer.setTimeout(this->request_timeout.count(),&Transmitter::timeout_event,this,message.second->message_id,message.second->ack_accept);
                            }
                        }
                    }
                }
            }
            // 等待响应，这个阶段可以进行移除message的事宜
            ssize_t recvLen = -1;
            struct sockaddr_in sender_address{};
            socklen_t len = 0;
            do{
                recvLen = ::recvfrom(socket_fd, read_buffer, Protocol::FullPieceSize + 1,0, (struct sockaddr*)&sender_address, &len);
                if (recvLen >= protocol.protocolHeaderSize){
                    bool isSuccess = false;
                    auto header = protocol.parse(read_buffer, recvLen, isSuccess);
                    if (isSuccess){
                        auto msg= messages.find(header.timePoint);
                        if (msg == messages.end()) continue;  // 没有历史消息
                        msg->second->last_active = GetTick(); // 更新活跃时间
                        msg->second->timeout_time = 0;
                        if (header.phase == CommunicationPhase::Request)
                        {
                            //收到确认ACK
                            if (header.type == ProtocolType::ReceiverACK){
                                //SPDLOG_INFO("Transmitter get ACK {} of {}!" , header.acceptOrder ,msg->second->message_id);
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
                                timer.setTimeout(this->request_timeout.count(), &Transmitter::timeout_event, this,  msg->second->message_id,  msg->second->ack_accept);
                            }// 服务器资源已经耗尽，无法再处理下去
                            else if (header.type == ProtocolType::TheServerResourcesExhausted){
                                msg->second->response_data.isSuccess = false;
                                msg->second->response_data.reason = FailureReason::TheServerResourcesExhausted;
                                try_trigger(msg->second);
                            }// 服务器返回心跳信息
                            else if (header.type == ProtocolType::TimedOutResponseHeartbeat){
                                //msg->second->timeout_time = 0; //已经统一执行了
                                //SPDLOG_INFO("Transmitter get ResponseHeartbeat from server!" , header.acceptOrder ,msg->second->message_id);
                            }
                                // 服务器请求心跳信息
                            else if (header.type == ProtocolType::TimedOutRequestHeartbeat){
                                send_heart_beat(msg->second, header.phase);
                            }
                        }else if(header.phase == CommunicationPhase::Response){
                            //网络并不会按序到达
                            if (msg->second->phase == CommunicationPhase::Request){
                                msg->second->phase = CommunicationPhase::Response;
                                msg->second->ack_accept = msg->second->piece_count;
                            }// 收到响应数据
                            else if (header.type == ProtocolType::RequestSend){
                                //SPDLOG_INFO("Transmitter get data order {} of {} from server!" , header.pieceOrder ,msg->second->message_id);
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
                            }// 服务端请求 ACK  [ 暂未纳入协议流程 ]
                            else if(header.type == ProtocolType::RequestACK){
                                auto ACK = msg->second->response_data.setPieceState(header.pieceOrder , true);
                                //发送ACK
                                send_response_ACK(msg->second, ACK);
                            }//收到心跳数据，服务器正在处理数据
                            else if(header.type == ProtocolType::TimedOutResponseHeartbeat){
                                //再等待一个超时时间
                                response_timeout_event(msg->first,  msg->second->last_active);
                                //SPDLOG_INFO("Transmitter get ResponseHeartbeat from server!" , header.acceptOrder ,msg->second->message_id);
                            }// 服务器请求心跳信息 [ 暂未纳入协议流程 ]
                            else if (header.type == ProtocolType::TimedOutRequestHeartbeat){
                                send_heart_beat(msg->second, CommunicationPhase::Response);
                            }
                            //其他情况不作回应
                        }
                    }
                }
            } while (recvLen  > 0);
            //处理定时器时间
            {
                std::lock_guard<std::mutex> lock(timer_mtx);
                timer.runTask();
            }
        }
        //循环读 直到读不到任何数据
    }

    bool Transmitter::send(TransmitterEvent&& event) {
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
                // 插入到任务队列
                if (messages.count(taskPtr->message_id) == 0)
                {
                    messages[taskPtr->message_id] =  taskPtr;
                    condition.notify_one();
                    return true;
                }
            }
        }
        return false;
    }

    void Transmitter::send_data(const std::shared_ptr<TransmitterTask>& task) {
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
            //SPDLOG_INFO("Transmitter send request data {} order {}", pieceDataPartSize, i);
        }
        task->ack_expect = i; //预期 ack_expect
    }

    void Transmitter::set_request_timeout(const uint32_t &_timeout) {
        this->request_timeout = std::chrono::milliseconds(_timeout);
    }

    void Transmitter::set_response_timeout(const uint32_t &_timeout) {
        this->response_timeout = std::chrono::milliseconds(_timeout);
    }

    void Transmitter::set_try_time(const uint16_t &_try) {
        this->try_time = _try;
    }

    void Transmitter::request_ACK(const std::shared_ptr<TransmitterTask>& task){
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

    void Transmitter::trigger(uint64_t message_id){
        auto it = messages.find(message_id);
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
                timer.setTimeout(0, &Transmitter::delete_message, this, message_id);
            }
        }
    }

    void Transmitter::try_trigger(const std::shared_ptr<TransmitterTask>& msg){
        if (!msg->is_trigger){
            msg->is_trigger = true;
            if(workers != nullptr){
                auto ex = make_executor(&Transmitter::trigger, this, msg->message_id);
                auto result = workers->commit_executor(ex);
                if (!result.isSuccess){
                    //如果提交失败咋办？
                    SPDLOG_ERROR("Transmitter::trigger commit to thread pool error!", msg->message_id);
                    //开线程
                    auto fu = std::async(std::launch::async,&Transmitter::trigger, this, msg->message_id);
                }
            }else{
                trigger(msg->message_id);
            }
        }
    }

    void Transmitter::response_timeout_event(uint64_t message_id, std::chrono::milliseconds last_active) {
        auto it = messages.find(message_id);
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
                    timer.setTimeout(this->request_timeout.count(), &Transmitter::response_timeout_event, this,  message_id, last_active);
                }
            }
        }
    }

    void Transmitter::timeout_event(uint64_t message_id, uint64_t cur_ack) {
        auto it = messages.find(message_id);
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
                    //SPDLOG_ERROR("Transmitter::trigger timout {}", it->second->message_id);
                    //再设置一个定时器
                    timer.setTimeout(this->request_timeout.count(), &Transmitter::timeout_event, this,  message_id, cur_ack);
                }
            }
        }
    }

    void Transmitter::delete_message(uint64_t message_id) {
        if (messages.count(message_id)){
            //SPDLOG_INFO("Transmitter delete message {} !", message_id);
            messages.erase(message_id);
        }
    }

    void Transmitter::send_response_ACK(const std::shared_ptr<TransmitterTask>& task, const uint16_t &_ack_number) {
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
    void Transmitter::send_heart_beat(const std::shared_ptr<TransmitterTask>& task, CommunicationPhase phase) {
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

    void Transmitter::send_request_heart_beat(const std::shared_ptr<TransmitterTask>& task, CommunicationPhase phase) {
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

    void Transmitter::start(TransmitterThreadType type) {
        run_state.store(true, std::memory_order_relaxed);
        if (type == TransmitterThreadType::Synchronous){
            loop();
        }else{
            try {
                runner = std::make_shared<std::thread>(&Transmitter::loop, this);
            }catch(const std::exception &exp){
                throw exp;
            }
        }
    }

    void Transmitter::stop() noexcept {
        while (!new_messages.empty()  || !messages.empty()){
            std::this_thread::sleep_for(10ms);
        }
        run_state.store(false, std::memory_order_relaxed);
        condition.notify_one();
    }

    void Transmitter::stop_immediately() noexcept {
        run_state.store(false, std::memory_order_relaxed);
        condition.notify_one();
    }

    Transmitter::Transmitter(bool flag ,int _socket_fd, const std::shared_ptr<ThreadPool> &_workers)
    :socket_fd(_socket_fd),workers(_workers){
        if(_socket_fd <= 0) throw ClientException("socket fd not right", ClientError::SocketFDError);;
        pool = MemoryPoolSingleton();
    }
} // muse::rpc