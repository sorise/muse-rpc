//
// Created by remix on 23-7-24.
//
#include "reactor_pool.hpp"

namespace muse::rpc{

    ThreadPool* Threads() {
        return new ThreadPool(
                REACTOR_MIN_THREAD,
                REACTOR_MAX_THREAD,
                REACTOR_QUEUE_LENGTH,
                ThreadPoolType::Flexible,
                ThreadPoolCloseStrategy::WaitAllTaskFinish,
                std::chrono::milliseconds(REACTOR_THREAD_RATE_millisecond)
        );
    }

    std::shared_ptr<ThreadPool> GetThreadPoolSingleton(){
        static std::shared_ptr<ThreadPool> pool = std::shared_ptr<ThreadPool>(Threads());
        return pool;
    }


}