//
// Created by remix on 23-7-8.
//
#ifndef MUSE_THREAD_POOL_HPP
#define MUSE_THREAD_POOL_HPP    1

#include <chrono>
#include <queue>

#include "executor_token.h"
#include "commit_result.hpp"
#include "conf.h"

using namespace std::chrono_literals;

// 只能传递引用、指针
namespace muse::pool{

    class ThreadPool{
    private:
        std::vector<std::shared_ptr<Worker>> workers;               //所有的工作线程存放位置
        std::once_flag initFlag;                                    //初始化
        unsigned int cores;                                         //线程池中的核心线程数量，线程池中线程最低数量
        unsigned int dynamicWorkerCount;                            //动态线程数量
        std::condition_variable condition;                          //条件变量 用于阻塞或者唤醒线程
        std::atomic<bool> runState{false };                       //线程池是否执行, 线程池可以被停止吗？
        std::atomic<int>  vacantThreadsCount{ 0 };                //空闲线程数量
        std::atomic<bool>  stopCommitState{false };               //停止提交
        std::atomic<bool>  isTerminated {false };                 //线程池是否已经关闭
        std::queue<std::shared_ptr<Executor>> queueTask;            //任务队列
        std::mutex queueMute;                                       //队列互斥量
        ThreadPoolCloseStrategy closeStrategy;                      //关闭策略
        std::chrono::milliseconds timeUnit;                         //管理线程运行频率
        std::shared_ptr<std::thread> managerThread{nullptr};        //管理线程
        bool isStart {false};                                       //线程池是否开始运行过
        ThreadPoolType Type;
        size_t QueueMaxSize {2048};
        size_t MaxThreadCount  {32};
        //核心线程运行逻辑
        void coreRun(unsigned int threadIndex);

        //动态线程运行逻辑
        void dynamicRun(unsigned int dynamicThreadIndex);

        /* 添加核心工作线程 */
        bool addCoreThread(unsigned int count) noexcept;

        /* 添加动态线程 */
        bool addDynamicThread() noexcept;

        /* 杀死动态线程 */
        bool killDynamicThread() noexcept;

        /* 初始化代码 */
        void init();

        /* 只有线程池类型是 Flexible 才可以调用 */
        void managerThreadTask();

    public:
        ThreadPool(size_t coreThreadsCount, size_t _maxThreadCount, size_t _queueMaxSize, ThreadPoolType _type,  ThreadPoolCloseStrategy poolCloseStrategy, std::chrono::milliseconds unit = 2500ms);

        ThreadPool(const ThreadPool& other) = delete; //不允许赋值

        ThreadPool(ThreadPool&& other) = delete; //不允许移动

        ThreadPool& operator=(const ThreadPool& other) = delete; //不允许赋值拷贝

        ThreadPool& operator=(ThreadPool&& other) = delete; //不允许赋值移动

        /* 提交多个任务 */
        std::vector<CommitResult> commit_executors(const std::vector<std::shared_ptr<Executor>>& tasks );

        std::vector<CommitResult> commit_executors(std::initializer_list<std::shared_ptr<Executor>> tasks);

        CommitResult commit_executor(const std::shared_ptr<Executor>& task);

        size_t taskSize() noexcept;

        //按照关闭策略 关闭 线程池
        std::queue<std::shared_ptr<Executor>> close();

        ~ThreadPool();
    };
}
#endif //MUSE_THREAD_POOL_HPP
