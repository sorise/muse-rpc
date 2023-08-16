//
// Created by remix on 23-8-16.
//

#ifndef MUSE_RPC_PEER_HPP
#define MUSE_RPC_PEER_HPP
#include <iostream>

namespace muse::rpc{
    struct SocketConnection{
        int socket {-1};
        uint32_t SubReactor_index {0};
    };

    class Peer {
    public:
        friend bool operator <(const Peer &me, const Peer &other);
    private:
        uint16_t client_port;   //端口号 主机字节序模式
        uint32_t client_ip_address; // IP 地址 主机字节序模式
    public:
        Peer(uint16_t _port, uint32_t _ip);
        Peer(const Peer &other) = default;
        virtual ~Peer() = default;
        [[nodiscard]] uint16_t getHostPort() const;
        [[nodiscard]] uint32_t getIpAddress() const;

        bool operator==(const Peer& oth) const;
    };
}

#endif //MUSE_RPC_PEER_HPP
