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
#include "../../logger/conf.hpp"
#include "invoker_exception.hpp"
#include <cstring>
#include "response_data.hpp"

//阻塞模式
namespace muse::rpc{
    class ResponseData;
    class ResponseDataFactory;

    //家庭小作坊模式
    class Invoker {
    public:
        //必须小于 100 0000 ，也就是 一秒 不然会出错
        static constexpr long timeout = 120000; //100000
        static constexpr int tryTimes = 3;
    private:
        struct sockaddr_in server_address;
        uint32_t line;
        uint64_t message_id;
        uint32_t total_size {0};
        uint16_t piece_count;
        uint16_t last_piece_size;
        const char* data_pointer {nullptr};
        uint16_t ack_accept {0};
        int socket_fd;
        Protocol protocol{};
        bool server_is_active {true};
    public:
        Invoker(const char * _ip_address, const uint16_t& port);

        Invoker(int _socket_fd, const char * _ip_address, const uint16_t& port);

        void setSocket(int _socket_fd);

        ResponseData request(const char *data, size_t data_size, ResponseDataFactory factory);

        static int getSendCount(const uint16_t& piece_count);

        ~Invoker();
    };


}


#endif //MUSE_CHANNEL_HPP
