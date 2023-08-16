//
// Created by remix on 23-7-20.
//

#ifndef MUSE_CHANNEL_HPP
#define MUSE_CHANNEL_HPP
#include <iostream>
#include <memory>
#include <thread>
#include "../protocol/protocol.hpp"
#include <arpa/inet.h>
#include "timer/timer_wheel.hpp"
#include "../logger/conf.hpp"
#include "invoker_exception.hpp"
#include <cstring>
#include "response_data.hpp"
#include "../server/zlib_service.hpp"
#include "../memory/conf.hpp"
#include "../server/middleware_channel.hpp"

//阻塞模式
namespace muse::rpc{
    //家庭小作坊模式
    class Invoker {
    public:
        //必须小于 100 0000 ，也就是 一秒 不然会出错
        //一阶段等待时间
        static constexpr long timeout = 500000; //0.5s
        //二阶段等待时间
        static constexpr long WaitingTimeout = 600000; //0.6s
        static constexpr int tryTimes = 3;
    private:
        struct sockaddr_in server_address{};
        int socket_fd;
        Protocol protocol{};
        void sendResponseACK(int _socket_fd, uint16_t _ack_number,  uint64_t _message_id);
        void sendHeartbeat(int _socket_fd, uint64_t _message_id);
        /* 请求服务器，是否还在处理数据 */
        void sendRequestHeartbeat(int _socket_fd, uint64_t _message_id);
    public:
        void Bind(uint16_t _local_port);
        Invoker(const char * _ip_address, const uint16_t& port);
        Invoker(int _socket_fd, const char * _ip_address, const uint16_t& port);
        void setSocket(int _socket_fd);
        ResponseData request(const char *_data, size_t data_size, ResponseDataFactory factory);
        ~Invoker();
    };
}


#endif //MUSE_CHANNEL_HPP
