//
// Created by remix on 23-8-13.
//

#ifndef MUSE_RPC_GLOBAL_ID_HPP
#define MUSE_RPC_GLOBAL_ID_HPP
#include <iostream>
#include <mutex>
#include <atomic>

namespace muse::rpc{

    std::chrono::microseconds GetNowTickMicroseconds();

    std::chrono::microseconds GetNowTickSeconds();

    /* 纳秒 ID message id */
    uint64_t GlobalMicrosecondsId();

    /* 用于 */
    uint64_t GlobalSecondsId();
}


#endif //MUSE_RPC_GLOBAL_ID_HPP
