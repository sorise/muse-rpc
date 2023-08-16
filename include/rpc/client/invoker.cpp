#include "invoker.hpp"

namespace muse::rpc{

    /* 构造函数 */
    Invoker::Invoker(const char * _ip_address, const uint16_t& port)
    :socket_fd(-1){
        if(1 != inet_aton(_ip_address,&server_address.sin_addr)){
            throw ClientException("ip address not right", ClientError::IPAddressError);
        }
        server_address.sin_family = PF_INET;
        server_address.sin_port = htons(port);
        socket_fd = socket(PF_INET, SOCK_DGRAM, 0);

        struct timeval tm = {0,Invoker::timeout};
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm, sizeof(struct timeval));

        if (socket_fd == -1) {
            throw ClientException("create the socket failed!", ClientError::CreateSocketError);
        }
    }

    /* 构造函数 */
    Invoker::Invoker(int _socket_fd, const char *_ip_address, const uint16_t &port) {
        socket_fd = _socket_fd;
        ::memset((char *)&server_address, 0, sizeof(server_address));
        if (socket_fd <= 0){
            throw ClientException("socket not right", ClientError::SocketFDError);
        }
        if(1 != inet_aton(_ip_address,&server_address.sin_addr)){
            throw ClientException("ip address not right", ClientError::IPAddressError);
        }
        server_address.sin_family = PF_INET;
        server_address.sin_port = htons(port);
        struct timeval tm = {0,Invoker::timeout};
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm, sizeof(struct timeval));
    }

    void Invoker::setSocket(int _socket_fd) {
        socket_fd = _socket_fd;
        if (socket_fd <= 0){
            throw ClientException("socket not right", ClientError::SocketFDError);
        }
    }

    Invoker::~Invoker(){
        if (socket_fd != -1) close(socket_fd);
    }

    ResponseData Invoker::request(const char *_data, size_t data_size, ResponseDataFactory factory) {
        if (data_size > 1024 * 1024 * 100){
            throw std::runtime_error("The data volume exceeds the 100M limit！");
        }
        std::shared_ptr<char[]> data_share(const_cast<char*>(_data), [](char *ptr){  });

        auto tpl =  MiddlewareChannel::GetInstance()->ClientOut(data_share, data_size, factory.getPool());
        //auto tpl =  zlib_service.Out(data_share, data_size, factory.getPool());

        std::shared_ptr<char[]> data = std::get<0>(tpl);
        data_size = std::get<1>(tpl);

        ResponseData response = factory.getResponseData();
        uint16_t  ack_accept = 0;

        // 总数据量
        uint32_t total_size = data_size;
        uint16_t piece_count =  data_size/Protocol::defaultPieceLimitSize + (((total_size % Protocol::defaultPieceLimitSize) == 0)?0:1);
        uint16_t last_piece_size = (total_size % Protocol::defaultPieceLimitSize == 0)? Protocol::defaultPieceLimitSize:total_size % Protocol::defaultPieceLimitSize ; /* 数据部分 */
        auto message_id = muse::timer::TimerWheel::GetTick().count();

        char buffer[Protocol::FullPieceSize + 1];         // 缓存区
        socklen_t len = sizeof(server_address);           // ok
        int times = Protocol::getSendCount(piece_count);  //获得发送次数

        int needTry = Invoker::tryTimes;    //超时尝试次数
        int receiveMessageCount = 1;

        struct timeval tm_phase_request = {0,Invoker::timeout};
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm_phase_request, sizeof(struct timeval));

        // 阶段 1
        while (ack_accept != piece_count){
            uint16_t end = ack_accept + times;
            if (receiveMessageCount != 0){
                // 发送数据给服务器
                for (uint16_t i = ack_accept; i < end && i < piece_count; ++i) {
                    protocol.initiateSenderProtocolHeader(buffer,message_id,data_size,Protocol::protocolHeaderSize);
                    protocol.setProtocolType(buffer,ProtocolType::RequestSend);
                    // 计算当前的 piece 的大小
                    uint16_t pieceDataPartSize = (i == (piece_count - 1))?last_piece_size: Protocol::defaultPieceLimitSize;
                    // 设置序号和大小
                    protocol.setProtocolOrderAndSize(buffer, i, pieceDataPartSize);
                    std::memcpy(buffer + Protocol::protocolHeaderSize, data.get() + (i * Protocol::defaultPieceLimitSize), pieceDataPartSize); //body
                    // 发送输出到服务器
                    ::sendto(socket_fd, buffer,  Protocol::protocolHeaderSize + pieceDataPartSize, 0, (struct sockaddr*)&server_address, len);
                    SPDLOG_INFO("Invoker send request data {} order {}", pieceDataPartSize, i);
                }
            }
            receiveMessageCount = 0;
            //等待响应
            for (uint16_t j = ack_accept; j < end && j < piece_count; ++j) {
                auto recvLength = recvfrom(socket_fd, buffer, Protocol::FullPieceSize + 1,0, (struct sockaddr*)&server_address, &len);
                if (recvLength > 0){
                    //如果在超时之前获得了数据
                    bool isSuccess = false;
                    ProtocolHeader header = protocol.parse(buffer,recvLength, isSuccess);
                    if (isSuccess){
                        if (header.timePoint != message_id){
                            SPDLOG_ERROR("get message id {} expect message id {}!", header.timePoint, message_id);
                            continue; //下一轮
                        }
                        receiveMessageCount++;
                        needTry = Invoker::tryTimes; //重置尝试次数，只有收到正确的数据才会重置
                        if (header.phase == CommunicationPhase::Request){
                            if (header.type == ProtocolType::ReceiverACK){
                                if (header.acceptOrder > ack_accept){
                                    ack_accept = header.acceptOrder;
                                }
                                SPDLOG_INFO("accept ACK {} from server", header.acceptOrder);
                                if (ack_accept == piece_count){
                                    break; //跳出 for
                                }
                            }else if (header.type == ProtocolType::TimedOutResponseHeartbeat){
                                //重置尝试次数 上面已经做了
                            }else if (header.type == ProtocolType::TheServerResourcesExhausted){
                                //服务器资源已经耗尽，无法接受处理
                                response.isSuccess = false;
                                response.reason = FailureReason::TheServerResourcesExhausted;
                                return response;
                            }else if (header.type == ProtocolType::StateReset){
                                //再次尝试 重新发送数据
                            }
                        }else{
                            //收到第二个阶段的消息
                            ack_accept = piece_count;
                        }
                    }else{
                        throw ClientException("server response data can not parse!", ClientError::ServerNotSupportTheProtocol);
                    }
                }else{
                    SPDLOG_ERROR("request timeout no data receive from server");
                    //超时没有收到消息
                    protocol.initiateSenderProtocolHeader(buffer,message_id,data_size,Protocol::protocolHeaderSize);
                    protocol.setProtocolType(buffer,ProtocolType::RequestACK);
                    ::sendto(socket_fd, buffer,  Protocol::protocolHeaderSize, 0, (struct sockaddr*)&server_address, len);
                    break;//很重要的
                }
            }

            if (receiveMessageCount == 0){
                needTry--;
                //多次尝试后还没有响应 表示超时了
                if (needTry <= 0){
                    response.isSuccess = false;
                    response.reason = FailureReason::NetworkTimeout;
                    return response;
                }
            }
        }

        //阶段 2
        SPDLOG_INFO("go in phase response");
        int responseNeedTry = Invoker::tryTimes;    //超时尝试次数

        //重新设置超时时间
        struct timeval tm_phase_response = {0,Invoker::WaitingTimeout};
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm_phase_response, sizeof(struct timeval));

        while (true){
            auto recvLength = recvfrom(socket_fd, buffer, Protocol::FullPieceSize + 1,0, (struct sockaddr*)&server_address, &len);
            if (recvLength > 0){
                bool isSuccess = false;
                ProtocolHeader header = protocol.parse(buffer,recvLength, isSuccess);
                if (isSuccess){
                    if (header.timePoint != message_id){
                        //不是我需要的返回消息,忽略
                        SPDLOG_ERROR("get message id {} expect message id {}!", header.timePoint, message_id);
                        continue; //下一轮
                    }else{
                        if (header.phase == CommunicationPhase::Request){
                            responseNeedTry = Invoker::tryTimes; //重置尝试次数，只有收到正确的数据才会重置
                        }
                        else if(header.phase == CommunicationPhase::Response){
                            responseNeedTry = Invoker::tryTimes; //重置尝试次数，只有收到正确的数据才会重置
                            if (header.type == ProtocolType::RequestSend){
                                SPDLOG_INFO("Get Data From Response Server order {}", header.pieceOrder);
                                //只会初始化一次
                                response.initialize(header.timePoint, header.totalCount, header.totalSize);
                                //保存数据
                                if (!response.getPieceState(header.pieceOrder)){
                                    auto des = response.data.get() +  header.pieceOrder * Protocol::defaultPieceLimitSize;
                                    // 完成报文内容拷贝
                                    std::memcpy(des, buffer + muse::rpc::Protocol::protocolHeaderSize ,header.pieceSize);
                                }
                                auto ACK = response.setPieceState(header.pieceOrder , true);
                                sendResponseACK(socket_fd, ACK, header.timePoint);
                                SPDLOG_INFO("send ACK {} to server total {}", ACK, response.piece_count);
                                if (ACK == response.piece_count){
                                    SPDLOG_INFO("get ALL response data!");
                                    break;
                                }
                            }else if(header.type == ProtocolType::RequestACK){
                                SPDLOG_INFO("Get RequestACK From Response Server");
                                uint16_t ackOrder = response.is_initial?response.getAckNumber():0;
                                sendResponseACK(socket_fd, ackOrder, header.timePoint);
                            }else if(header.type == ProtocolType::TimedOutResponseHeartbeat){
                                SPDLOG_INFO("Get TimedOutResponseHeartbeat From Response Server");
                                //重置 needTry  上面已经重置了
                                SPDLOG_INFO("waiting server to process");
                            }else if(header.type == ProtocolType::TimedOutRequestHeartbeat){
                                SPDLOG_INFO("Get TimedOutRequestHeartbeat From Response Server");
                                sendHeartbeat(socket_fd, header.timePoint);
                            }else{
                                //没必要处理
                                SPDLOG_INFO("accept a error type message {}", typeid(header.type).name());
                            }
                        }
                    }
                }
            }
            else{
                responseNeedTry--;
                sendRequestHeartbeat(socket_fd, message_id);
                SPDLOG_WARN("response send request heart beat");
                //多次尝试后还没有响应 表示超时了
                if (responseNeedTry <= 0){
                    response.isSuccess = false;
                    response.reason = FailureReason::NetworkTimeout;
                    SPDLOG_ERROR("phase response timeout no data receive from server");
                    return response;
                }
            }
        }

        //属于是真获得了数据
        auto realTpl = MiddlewareChannel::GetInstance()->ClientIn(response.data, response.total_size, factory.getPool());
        response.data = std::get<0>(realTpl);
        response.total_size = std::get<1>(realTpl);
        response.isSuccess = true;
        return response;
    }

    void Invoker::sendResponseACK(int _socket_fd, uint16_t _ack_number, uint64_t _message_id) {
        char buffer[muse::rpc::Protocol::protocolHeaderSize];
        protocol.initiateSenderProtocolHeader(
                buffer,
                _message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol.setProtocolType(
                buffer,
                ProtocolType::ReceiverACK
        );
        protocol.setProtocolPhase(buffer,CommunicationPhase::Response);
        protocol.setACKAcceptOrder(buffer, _ack_number);
        ::sendto(_socket_fd, buffer,  Protocol::protocolHeaderSize, 0, (struct sockaddr*)&server_address, sizeof(server_address));
    }

    //发送心跳信息
    void Invoker::sendHeartbeat(int _socket_fd, uint64_t _message_id) {
        char buffer[muse::rpc::Protocol::protocolHeaderSize];
        protocol.initiateSenderProtocolHeader(
                buffer,
                _message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol.setProtocolType(
                buffer,
                ProtocolType::TimedOutResponseHeartbeat
        );
        protocol.setProtocolPhase(buffer,CommunicationPhase::Response);
        ::sendto(_socket_fd, buffer,  Protocol::protocolHeaderSize, 0, (struct sockaddr*)&server_address, sizeof(server_address));
    }

    void Invoker::sendRequestHeartbeat(int _socket_fd, uint64_t _message_id) {
        char buffer[muse::rpc::Protocol::protocolHeaderSize];
        protocol.initiateSenderProtocolHeader(
                buffer,
                _message_id,
                0,
                muse::rpc::Protocol::protocolHeaderSize
        );
        protocol.setProtocolType(
                buffer,
                ProtocolType::TimedOutRequestHeartbeat
        );
        protocol.setProtocolPhase(buffer,CommunicationPhase::Response);
        ::sendto(_socket_fd, buffer,  Protocol::protocolHeaderSize, 0, (struct sockaddr*)&server_address, sizeof(server_address));
    }

    void Invoker::Bind(uint16_t _local_port) {
        //服务器地址
        sockaddr_in serverAddress{
                PF_INET,
                htons(_local_port),
                htonl(INADDR_ANY)
        };
        int on = 1;
        //地址复用 端口复用
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        //地址复用 端口复用
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

        //绑定端口号 和 IP
        if(bind(socket_fd, (const struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1){
            SPDLOG_ERROR("socket bind port {} error, errno {}", _local_port, errno);
            throw std::runtime_error("local port has been use by other program!");
        }
    }
}