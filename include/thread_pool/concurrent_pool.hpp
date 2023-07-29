/*
 * 基于 https://github.com/cameron314/concurrentqueue 提供的无所队列
 * */
#ifndef MUSE_CONCURRENT_QUEUE_POOL_HPP
#define MUSE_CONCURRENT_QUEUE_POOL_HPP
#include <chrono>
#include <memory>
#include <queue>
#include <exception>
#include <list>
#include "concurrentqueue.h"
#include "executor_token.h"
#include "commit_result.hpp"

using namespace moodycamel;
using namespace std::chrono_literals;

// 只能传递引用、指针
namespace muse::pool{
    //如果你的服务以几十万 上百万级运行，CPU一直运转，几乎不停止，可以使用此物。

    class ConcurrentThreadPool{
    private:
        size_t QueueMaxSize;                                                        //队列长度
        ConcurrentQueue<std::shared_ptr<Executor>> queueTask;        //存放任务
        std::vector<std::shared_ptr<ConcurrentWorker>> workers;                             //所有的工作线程存放位置
        std::mutex workersMutex;                                             //互斥量
        std::atomic<int> vacantSlotCount {0};                              //空闲
        ThreadPoolCloseStrategy closeStrategy;                               //关闭策略
        std::once_flag initialFlag;                                          //初始化
        std::shared_ptr<std::thread> manager{nullptr};                       //管理线程
        bool isStart {false};                                                //线程池是否开始运行过
        unsigned int cores;                                                  //线程池中的核心线程数量，线程池中线程最低数量
        std::atomic<bool>  runState{false };                               //线程池是否执行, 线程池可以被停止吗？
        std::atomic<bool>  stopCommitState{false };                        //停止提交
        std::atomic<bool>  isTerminated {false };                          //线程池是否已经关闭
        std::condition_variable condition;                                   //条件变量 用于阻塞或者唤醒线程
        std::chrono::milliseconds leisureTimeUnit;                           //空闲

        /* 核心线程工作流程 */
        void coreRun(unsigned int threadIndex);

        /* 添加核心工作线程 */
        bool addCoreThread(unsigned int count);

        //私有方法：
        bool initialize();

        bool tryAddThread();
    public:

        ConcurrentThreadPool(size_t coreThreadsCount, size_t _queueMaxSize , ThreadPoolCloseStrategy poolCloseStrategy, std::chrono::milliseconds leisureUnit = 1350ms);

        ConcurrentThreadPool(const ConcurrentThreadPool& other) = delete; //不允许赋值

        ConcurrentThreadPool(ConcurrentThreadPool&& other) = delete; //不允许移动

        ConcurrentThreadPool& operator=(const ConcurrentThreadPool& other) = delete; //不允许赋值拷贝

        ConcurrentThreadPool& operator=(ConcurrentThreadPool&& other) = delete; //不允许赋值移动

        /* 提交多个任务 */
        std::vector<CommitResult> commit_executors(const std::vector<std::shared_ptr<Executor>>& tasks );

        /* 提交多个任务 */
        std::vector<CommitResult> commit_executors(const std::list<std::shared_ptr<Executor>>& tasks );

        /* 提交多个任务 */
        std::vector<CommitResult> commit_executors(std::initializer_list<std::shared_ptr<Executor>> tasks);

        CommitResult commit_executor(const std::shared_ptr<Executor>& task);

        //按照关闭策略 关闭 线程池
        std::queue<std::shared_ptr<Executor>> close();

        ~ConcurrentThreadPool();
    };

}

#endif //MUSE_CONCURRENT_QUEUE_POOL_HPP
