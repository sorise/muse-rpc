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

#define REACTOR_QUEUE_LENGTH 4096

//管理线程运行频率
#define REACTOR_THREAD_RATE_millisecond 3000

namespace muse::rpc{
    //为了线程安全
    extern std::shared_ptr<ThreadPool> GetThreadPoolSingleton();

    class ThreadPoolSetting{
    public:
        //线程池核心线程数
        static size_t MinThreadCount;
        //线程池最大线程数
        static size_t MaxThreadCount;
        //任务缓存队列最大程度
        static size_t TaskQueueLength;
        //允许动态线程空闲时间,毫秒数
        static std::chrono::milliseconds DynamicThreadVacantMillisecond;
    };
}



#endif //MUSE_RPC_REACTOR_POOL_HPP
