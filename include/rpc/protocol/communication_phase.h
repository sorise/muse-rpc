//
// Created by remix on 23-7-23.
//

#ifndef MUSE_RPC_COMMUNICATION_PHASE_H
#define MUSE_RPC_COMMUNICATION_PHASE_H
#include <iostream>

namespace muse::rpc{
    enum class CommunicationPhase:uint8_t {
        Request = 0, //client 向 server 发送请求数据
        Response = 1 //server 向 client 发送响应数据
    };
}


#endif //MUSE_RPC_COMMUNICATION_PHASE_H
