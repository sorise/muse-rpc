//
// Created by remix on 23-7-20.
//

#ifndef MUSE_VIRTUAL_CONNECTION_HPP
#define MUSE_VIRTUAL_CONNECTION_HPP
#include <chrono>

namespace muse::rpc{

    //标识一个虚拟连接
    struct VirtualConnection{
        int socket_fd { -1 };
        std::chrono::milliseconds last_active {0};
        sockaddr_in addr;
        

    };
}

#endif //MUSE_VIRTUAL_CONNECTION_HPP
