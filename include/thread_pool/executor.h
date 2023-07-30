#include <thread>
#include <algorithm>
#include <iostream>
#include "conf.h"
#include <functional>

#ifndef MUSE_THREAD_POOL_EXECUTOR_H
#define MUSE_THREAD_POOL_EXECUTOR_H  1

namespace muse::pool{
    using Task = std::function<void()>; //表示一个代执行任务

    /* 执行任务 */
    class Executor{
        friend class ThreadPool;
        friend class ConcurrentThreadPool;
    public:
        explicit Executor(Task);
        Executor(const Executor& other) = delete;
        Executor(Executor&& other) noexcept;
        virtual ~Executor() = default;
    protected:
        Task task;
        bool finishState;                       //任务是否已经完成
        bool discardState;                      //是否被丢弃
        bool haveException;                     //是有具有异常
    };


}

#endif //MUSE_THREAD_POOL_EXECUTOR_H
