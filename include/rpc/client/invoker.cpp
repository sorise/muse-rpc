#include "invoker.hpp"

namespace muse::rpc{

    /* 构造函数 */
    Invoker::Invoker(const char * _ip_address, const uint16_t& port)
    :line(0), socket_fd(-1){
        if(1 != inet_aton(_ip_address,&server_address.sin_addr)){
            throw InvokerException("ip address not right", InvokerError::IPAddressError);
        }

        server_address.sin_family = PF_INET;
        server_address.sin_port = htons(port);
        socket_fd = socket(PF_INET, SOCK_DGRAM, 0);

        struct timeval tm = {0,Invoker::timeout};
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm, sizeof(struct timeval));

        if (socket_fd == -1) {
            throw InvokerException("create the socket failed!", InvokerError::CreateSocketError);
        }
    }

    /* 构造函数 */
    Invoker::Invoker(int _socket_fd, const char *_ip_address, const uint16_t &port) {
        socket_fd = _socket_fd;
        ::memset((char *)&server_address, 0, sizeof(server_address));
        if (socket_fd <= 0){
            throw InvokerException("socket not right", InvokerError::SocketFDError);
        }
        if(1 != inet_aton(_ip_address,&server_address.sin_addr)){
            throw InvokerException("ip address not right", InvokerError::IPAddressError);
        }
        server_address.sin_family = PF_INET;
        server_address.sin_port = htons(port);

        struct timeval tm = {0,Invoker::timeout};
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm, sizeof(struct timeval));
    }

    void Invoker::setSocket(int _socket_fd) {
        socket_fd = _socket_fd;
        if (socket_fd <= 0){
            throw InvokerException("socket not right", InvokerError::SocketFDError);
        }
    }

    Invoker::~Invoker(){
        if (socket_fd != -1) close(socket_fd);
        data_pointer = nullptr;
    }

    ResponseData Invoker::request(const char *data, size_t data_size, ResponseDataFactory factory) {
        ResponseData response = factory.getResponseData();

        line = 0;
        // 总数据量
        total_size = data_size;
        piece_count =  data_size/Protocol::defaultPieceLimitSize + (((data_size % Protocol::defaultPieceLimitSize) == 0)?0:1);
        last_piece_size = (data_size % Protocol::defaultPieceLimitSize == 0)? Protocol::defaultPieceLimitSize:data_size % Protocol::defaultPieceLimitSize ; /* 数据部分 */
        data_pointer = data;
        message_id = muse::timer::TimerWheel::GetTick().count();

        char buffer[Protocol::FullPieceSize + 1];        // 缓存区
        socklen_t len = sizeof(server_address);          //ok
        int times = Invoker::getSendCount(piece_count);  //获得等待次数


        int needTry = Invoker::tryTimes;
        //阶段 1
        while (ack_accept != piece_count){
            uint16_t start = ack_accept + times;
            //发送数据给服务器
            for (uint16_t i = ack_accept; i < start && i < piece_count; ++i) {
                protocol.initiateSenderProtocolHeader(buffer,message_id,data_size,Protocol::protocolHeaderSize);
                protocol.setProtocolType(buffer,ProtocolType::RequestSend);
                //计算当前的 piece 的大小
                uint16_t pieceDataPartSize = (i == (piece_count - 1))?last_piece_size: Protocol::defaultPieceLimitSize;

                protocol.setProtocolOrderAndSize(buffer, i, pieceDataPartSize);
                std::memcpy(buffer + Protocol::protocolHeaderSize, data + line, pieceDataPartSize); //body
                ::sendto(socket_fd, buffer,  Protocol::protocolHeaderSize + pieceDataPartSize, 0, (struct sockaddr*)&server_address, len);
                line += pieceDataPartSize;
                SPDLOG_INFO("send data {} order {}", pieceDataPartSize, i);
            }

            int receiveMessageCount = 0;
            //等待响应
            for (uint16_t j = ack_accept; j < start && j < piece_count; ++j) {
                auto recvLength = recvfrom(socket_fd, buffer, Protocol::FullPieceSize + 1,0, (struct sockaddr*)&server_address, &len);
                if (recvLength > 0){
                    needTry = Invoker::tryTimes;
                    receiveMessageCount++;
                    bool isSuccess = false;
                    ProtocolHeader header = protocol.parse(buffer,recvLength, isSuccess);
                    if (isSuccess){
                        if (header.timePoint != message_id){
                            //不是我需要的返回消息，这说明服务器故障
                            response.isSuccess = false;
                            response.reason = FailureReason::TheRunningLogicOfTheServerIncorrect;

                            SPDLOG_ERROR("get message id {} expect message id {}!", header.timePoint, message_id);
                            return response;
                        }
                        if (header.phase == CommunicationPhase::Request){
                            if (header.type == ProtocolType::ReceiverACK){
                                if (header.acceptOrder > ack_accept){
                                    ack_accept = header.acceptOrder;
                                }
                                SPDLOG_INFO("accept ACK {} from server", header.acceptOrder);
                            }else if (header.type == ProtocolType::TimedOutResponseHeartbeat){
                                server_is_active = true;
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
                        throw InvokerException("server response data can not parse!", InvokerError::ServerNotSupportTheProtocol);
                    }
                }else{
                    SPDLOG_ERROR("timeout no data receive from server");
                    //超时没有收到消息
                    protocol.initiateSenderProtocolHeader(buffer,message_id,data_size,Protocol::protocolHeaderSize);
                    protocol.setProtocolType(buffer,ProtocolType::RequestACK);
                    ::sendto(socket_fd, buffer,  Protocol::protocolHeaderSize, 0, (struct sockaddr*)&server_address, len);
                    break;
                }
            }

            if (receiveMessageCount == 0){
                needTry--;
                //多次尝试后还没有响应 表示超时了
                if (needTry == 0){
                    response.isSuccess = false;
                    response.reason = FailureReason::NetworkTimeout;
                    return response;
                }
            }
        }
        //阶段 2

        response.isSuccess = true;
        return response;
    }

    int Invoker::getSendCount(const uint16_t& piece_count) {
        if (piece_count <= 3){
            return 1;
        }else if (piece_count > 3 && piece_count <= 10){
            return 3;
        }else if (piece_count > 10 && piece_count <= 1024){
            return 5;
        }else{
            return 7;
        }
    }


}