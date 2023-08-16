//
// Created by remix on 23-7-19.
//

#ifndef MUSE_SERVER_REQUEST_HPP
#define MUSE_SERVER_REQUEST_HPP
#include <iostream>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>
#include <chrono>
#include "reactor_exception.hpp"
#include "../logger/conf.hpp"

using namespace std::chrono_literals;

namespace muse::rpc{

    static std::chrono::milliseconds GetNowTick(){
        std::chrono::time_point<std::chrono::system_clock> tp =
                std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
    }


    class Servlet {
    public:
        friend bool operator <(const Servlet &me, const Servlet &other);
    private:
        uint64_t request_id; //标识一个 client requestId
        uint16_t client_port;   //端口号 主机字节序模式
        uint32_t client_ip_address; // IP 地址 主机字节序模式
    public:
        Servlet(uint64_t _id, uint16_t _port, uint32_t _ip);
        Servlet(const Servlet &other) = default;
        virtual ~Servlet() = default;
        uint64_t getID() const;
        uint16_t getHostPort() const;
        uint32_t getIpAddress() const;
    };

    /* 标识一个用户的完整报文 */
    class Request: public Servlet{
    private:
        std::vector<bool>               piece_state;         // 分片状态
        uint16_t                        piece_count;         // 有多少个分片
        uint32_t                        total_data_size;     // 总共有多少数据
        int                             socketFd {-1};       // socket
        bool                            is_success{false};          //所有的数据都已经搞定了
        bool                            is_trigger{false};   //是否触发过
    public:
        std::chrono::milliseconds       active_dt {0};           // 上次活跃时间
        std::shared_ptr<char[]>         data;
        Request(uint64_t _id, uint16_t _port, uint32_t _ip, uint16_t _pieces, uint32_t _data_size);
        Request(const Request& other) = default;
        uint32_t getTotalDataSize() const;
        uint16_t getPieceCount() const;
        bool getPieceState(const uint16_t & _idx) const;
        uint16_t setPieceState(const uint16_t & _index, bool value);
        void setSocket(int _socket_fd);
        uint16_t getAckNumber();
        int getSocket() const;
        /* 防止多次触发处理事件 */
        void trigger();
        bool getTriggerState() const;
    };
}

#endif //MUSE_SERVER_REQUEST_HPP
