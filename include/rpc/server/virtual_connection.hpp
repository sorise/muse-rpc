//
// Created by remix on 23-7-20.
//
#ifndef MUSE_VIRTUAL_CONNECTION_HPP
#define MUSE_VIRTUAL_CONNECTION_HPP
#include <chrono>
#include <list>
#include "response.hpp"

namespace muse::rpc{
    //标识一个虚拟连接
    struct VirtualConnection{
        int socket_fd { -1 };
        std::chrono::milliseconds last_active {0};
        sockaddr_in addr;
        //std::shared_mutex mtx;//读写锁
        std::unordered_map<uint64_t,Response> responses { };
        std::list<uint64_t> messages { };
    };
}

#endif //MUSE_VIRTUAL_CONNECTION_HPP
