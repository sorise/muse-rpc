//
// Created by remix on 23-8-22.
//

#include "transmitter_link_reactor.hpp"

namespace muse::rpc{
    void TransmitterLinkReactor::loop() {
        SPDLOG_INFO("TransmitterLinkReactor Start!");
        is_start.store(true, std::memory_order_release);
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
                    timer.setTimeout(this->request_timeout.count(),&TransmitterLinkReactor::timeout_event,this,message.second->message_id,message.second->ack_accept);
                }else{
                    //数据还处于 1 阶段、表示数据还没有发送完毕
                    if (message.second->phase == CommunicationPhase::Request){
                        //消息还没有进入删除阶段
                        if (message.second->timeout_time < try_time){
                            // 判断是否收到回信、收到就发送
                            if(message.second->ack_accept > message.second->ack_expect - message.second->send_time){
                                send_data(message.second);
                                //发送后设置超时定时器
                                timer.setTimeout(this->request_timeout.count(),&TransmitterLinkReactor::timeout_event,this,message.second->message_id,message.second->ack_accept);
                            }
                        }
                    }
                }
            }
            // 等待响应，这个阶段可以进行移除message的事宜
            size_t recvLen = -1;
            struct sockaddr_in sender_address{};

            //等待新数据哈
            std::unique_lock<std::mutex> unique_lock(wait_new_data_mtx);
            waiting_newData_condition.wait_for(uniqueLock, recv_gap , [this]{
                if(!run_state.load(std::memory_order::memory_order_relaxed))
                    return true;
                return !newData.empty();
            });
            {
                std::lock_guard<std::mutex> lock(queue_mtx);
                while(!newData.empty()){
                    //ID
                    uint64_t msg_id = std::get<0>(newData.front());
                    //复制的数据
                    std::shared_ptr<char[]> data = std::get<1>(newData.front());
                    //数据长度
                    recvLen = std::get<2>(newData.front());
                    //地址信息
                    sender_address = std::get<3>(newData.front());

                    if (recvLen >= protocol.protocolHeaderSize){
                        bool isSuccess = false;
                        auto header = protocol.parse(data.get(), recvLen, isSuccess);
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
                                            SPDLOG_INFO("TransmitterLinkReactor send all data of the message_id {}", header.timePoint);
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
                                    timer.setTimeout(this->request_timeout.count(), &TransmitterLinkReactor::timeout_event, this,  msg->second->message_id,  msg->second->ack_accept);
                                }// 服务器资源已经耗尽，无法再处理下去
                                else if (header.type == ProtocolType::TheServerResourcesExhausted){
                                    msg->second->response_data.isSuccess = false;
                                    msg->second->response_data.reason = FailureReason::TheServerResourcesExhausted;
                                    try_trigger(msg->second);
                                }// 服务器返回心跳信息
                                else if (header.type == ProtocolType::TimedOutResponseHeartbeat){
                                    //msg->second->timeout_time = 0; //已经统一执行了
                                    SPDLOG_INFO("Transmitter get ResponseHeartbeat from server!" , header.acceptOrder ,msg->second->message_id);
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
                                        std::memcpy(des, data.get() + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
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
                    newData.pop();
                };
            }
            //处理定时器时间
            {
                std::lock_guard<std::mutex> lock(timer_mtx);
                timer.runTask();
            }
        }
    }

    TransmitterLinkReactor::~TransmitterLinkReactor() {
        run_state.store(false, std::memory_order::memory_order_release);
        waiting_newData_condition.notify_one();
        SPDLOG_INFO("TransmitterLinkReactor End!");
    }

    bool TransmitterLinkReactor::send(TransmitterEvent &&event, uint64_t msg_id) {
        //判断是否是多次发送同一个 event
        if (event.get_callBack_state() && event.get_remote_state()){
            //创建一个任务
            std::shared_ptr<TransmitterTask> taskPtr = std::make_shared<TransmitterTask>(std::move(event), msg_id);
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

    TransmitterLinkReactor::TransmitterLinkReactor(int _socket_fd, const std::shared_ptr<ThreadPool> &_workers)
   :Transmitter(true,_socket_fd, _workers) {

    }

    void TransmitterLinkReactor::acceptNewData(uint64_t id, std::shared_ptr<char[]> data, size_t size, sockaddr_in addr) {
        if (this->run_state.load(std::memory_order_relaxed)){
            {
                //加入队列中
                std::lock_guard<std::mutex> lock(queue_mtx);
                newData.emplace(id, data, size,  addr);
            }
        }
    }

    void TransmitterLinkReactor::try_trigger(const std::shared_ptr<TransmitterTask> &msg) {
        if (!msg->is_trigger){
            msg->is_trigger = true;
            if(workers != nullptr){
                //收到所有的数据了，删掉它
                Servlet servlet(msg->message_id, ntohs(msg->server_address.sin_port), ntohl(msg->server_address.sin_addr.s_addr));

                GlobalEntry::timerTree->setTimeout(0,[](const Servlet _servlet){
                    std::lock_guard<std::mutex> lock(GlobalEntry::mtx_active_queue);
                    GlobalEntry::active_queue->erase(_servlet);
                }, servlet);

                auto ex = make_executor(&TransmitterLinkReactor::trigger, this, msg->message_id);
                auto result = workers->commit_executor(ex);
                if (!result.isSuccess){
                    //如果提交失败咋办？
                    SPDLOG_ERROR("Transmitter::trigger commit to thread pool error!", msg->message_id);
                    //开线程
                    auto fu = std::async(std::launch::async,&TransmitterLinkReactor::trigger, this, msg->message_id);
                }
            }else{
                trigger(msg->message_id);
            }
        }
    }

    bool TransmitterLinkReactor::start_finish() {
        return is_start.load(std::memory_order_relaxed);
    }

}
