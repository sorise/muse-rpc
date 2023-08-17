//
// Created by remix on 23-7-24.
//
#include "reactor_pool.hpp"

namespace muse::rpc{

    size_t ThreadPoolSetting::MinThreadCount = REACTOR_MIN_THREAD;
    size_t ThreadPoolSetting::MaxThreadCount = REACTOR_MAX_THREAD;
    size_t ThreadPoolSetting::TaskQueueLength = REACTOR_QUEUE_LENGTH;
    std::chrono::milliseconds ThreadPoolSetting::DynamicThreadVacantMillisecond = std::chrono::milliseconds(REACTOR_THREAD_RATE_millisecond) ;


    ThreadPool* Threads() {
        return new ThreadPool(
                ThreadPoolSetting::MinThreadCount,
                ThreadPoolSetting::MaxThreadCount,
                ThreadPoolSetting::TaskQueueLength,
                ThreadPoolType::Flexible,
                ThreadPoolCloseStrategy::WaitAllTaskFinish,
                std::chrono::milliseconds(ThreadPoolSetting::DynamicThreadVacantMillisecond)
        );
    }

    std::shared_ptr<ThreadPool> GetThreadPoolSingleton(){
        static std::shared_ptr<ThreadPool> pool = std::shared_ptr<ThreadPool>(Threads());
        return pool;
    }


}