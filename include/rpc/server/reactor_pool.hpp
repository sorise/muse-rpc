//
// Created by remix on 23-7-24.
//

#ifndef MUSE_RPC_REACTOR_POOL_HPP
#define MUSE_RPC_REACTOR_POOL_HPP
#include "thread_pool/pool.hpp"

using namespace muse::pool;

//核心线程数
#define REACTOR_MIN_THREAD 4
//最大线程数
#define REACTOR_MAX_THREAD 4

#define REACTOR_QUEUE_LENGTH 2048

//管理线程运行频率
#define REACTOR_THREAD_RATE_millisecond 3000

namespace muse::rpc{
    //为了线程安全
    extern std::shared_ptr<ThreadPool> GetThreadPoolSingleton();

}



#endif //MUSE_RPC_REACTOR_POOL_HPP
