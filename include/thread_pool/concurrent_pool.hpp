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
#include "executor_token.hpp"
#include "commit_result.hpp"

using namespace moodycamel;
using namespace std::chrono_literals;

// 只能传递引用、指针
namespace muse::pool{
    //如果你的服务以几十万 上百万级运行，CPU一直运转，几乎不停止，可以使用此物。
    template<size_t QueueMaxSize = 2048>
    class ConcurrentThreadPool{
    private:
        ConcurrentQueue<std::shared_ptr<Executor>> queueTask {QueueMaxSize};        //存放任务
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
        void coreRun(unsigned int threadIndex){
            while (runState.load(std::memory_order_relaxed)){
                std::shared_ptr<Executor> executor;
                bool hasNewTask = queueTask.try_dequeue(executor);
                if(hasNewTask){
                    workers[threadIndex]->lastRunHaveTask = true;
                    //任务没有被执行
                    if (!executor->finishState){
                        //执行任务，防止异常导致 线程池崩溃！
                        try {
                            //正常执行完成状态
                            executor->task();
                        }catch(...){
                            //如果执行失败，一般是捕获不到的！
                            executor->haveException = true;
                        }
                        executor->finishState = true;
                    }
                }else{
                    //没有新任务了
                    if (workers[threadIndex]->lastRunHaveTask){
                        workers[threadIndex]->lastRunHaveTask = false;
                        //记录多场时间没有工作了
                        workers[threadIndex]->noWorkTime = GetTick();
                    }else{
                        std::chrono::milliseconds Gap = GetTick() -  workers[threadIndex]->noWorkTime;
                        if (Gap > leisureTimeUnit){
                            //标记当前线程已经死掉了
                            workers[threadIndex]->isRunning.store(false, std::memory_order_release);
                            //有一个线程死掉了
                            vacantSlotCount.fetch_add(1,std::memory_order_release);

                            printf("%d thread die !\n", threadIndex);
                            return;
                        }
                    }
                    std::this_thread::yield();
                }
            }
            printf("%d thread die !\n", threadIndex);
        }

        /* 添加核心工作线程 */
        bool addCoreThread(unsigned int count){
            try{
                for(unsigned int i = 0; i < count; i++){
                    //追加核心工作线程
                    std::shared_ptr<std::thread> wr(new std::thread(&ConcurrentThreadPool::coreRun, this, i));
                    workers[i]->isRunning.store(true, std::memory_order_release);
                    workers[i]->workman = wr;
                    //增加了一个线程
                }
                return true;
            }catch(const std::bad_alloc & ex){
                //内存不足以分配
                return false;
            }catch(const std::exception & ex){
                //其他异常
                return false;
            }catch(...){
                //观测失败
                return false;
            }
        }

        //私有方法：
        bool initialize(){
            try {
                //标志线程池已经初始化
                isStart = true;
                //如果是弹性线程池
                workers.resize(cores);
                for (int i = 0; i < cores; ++i) {
                    workers[i] = std::make_shared<ConcurrentWorker>();
                }
                //线程开始运行
                runState.store(true, std::memory_order_release);
                //添加核心工作线程
                if(!addCoreThread(cores)){
                    throw std::bad_alloc();
                }
            }catch(const std::bad_alloc & ex){
                std::cout << "memory not enough" << std::endl;
                throw ex;
            }catch(const std::exception & ex){
                throw ex;
            }catch(...){
                //观测失败
                return false;
            }
            return true;
        }

        bool tryAddThread(){
            //双重检测
            try{
                if (vacantSlotCount.load(std::memory_order_relaxed) > 0){
                    std::lock_guard<std::mutex> lock(workersMutex);
                    for (int i = 0; i < cores; ++i) {
                        std::shared_ptr<ConcurrentWorker> worker = workers[i];
                        if(!worker->isRunning.load(std::memory_order_relaxed)){
                            // 找到添加位置
                            //等上一个线程死掉
                            if (worker->workman != nullptr && worker->workman->joinable()){
                                worker->workman->join();
                            }
                            worker->lastRunHaveTask = true;
                            worker->isRunning.store(true, std::memory_order_relaxed);
                            //可能抛出异常
                            std::shared_ptr<std::thread> wr(new std::thread(&ConcurrentThreadPool::coreRun, this, i));
                            workers[i]->workman = wr;
                            workers[i]->isRunning.store(true, std::memory_order_release);

                            printf("%d thread add success!\n", i);
                            break;
                        }
                    }
                }
            }catch(const std::bad_alloc & ex){
                //内存不足以分配
                return false;
            }catch(const std::exception & ex){
                //其他异常
                return false;
            }catch(...){
                //观测失败
                return false;
            }
            return true;
        }


    public:

        explicit ConcurrentThreadPool(size_t coreThreadsCount, ThreadPoolCloseStrategy poolCloseStrategy, std::chrono::milliseconds leisureUnit = 1750ms)
        :cores(coreThreadsCount),
        closeStrategy(poolCloseStrategy),
        leisureTimeUnit(leisureUnit)
        {
            cores = (cores <= 0)?4:cores; //初始化线程数量
        }

        ConcurrentThreadPool(const ConcurrentThreadPool& other) = delete; //不允许赋值

        ConcurrentThreadPool(ConcurrentThreadPool&& other) = delete; //不允许移动

        ConcurrentThreadPool& operator=(const ConcurrentThreadPool& other) = delete; //不允许赋值拷贝

        ConcurrentThreadPool& operator=(ConcurrentThreadPool&& other) = delete; //不允许赋值移动

        /* 提交多个任务 */
        std::vector<CommitResult> commit_executors(const std::vector<std::shared_ptr<Executor>>& tasks ){
            //懒加载 初始化
            std::call_once(initialFlag, [this]{
                initialize();
            });
            //是否已经停止运行
            std::vector<CommitResult> results(tasks.size(), {false, RefuseReason::ThreadPoolHasStoppedRunning});
            if (stopCommitState.load(std::memory_order_relaxed)){
                return results;
            }
            int leftSlot = QueueMaxSize -queueTask.size_approx();
            auto success = queueTask.enqueue_bulk(tasks.begin(), leftSlot);
            if (success){
                for(auto & result : results) {
                    result.reason = RefuseReason::NoRefuse;
                    result.isSuccess = true;
                }
                tryAddThread(); //尝试添加线程
            }else{
                for(auto & result : results) {
                    result.reason = RefuseReason::MemoryNotEnough;
                }
            }
            return results;
        }

        /* 提交多个任务 */
        std::vector<CommitResult> commit_executors(const std::list<std::shared_ptr<Executor>>& tasks ){
            //懒加载 初始化
            std::call_once(initialFlag, [this]{
                initialize();
            });
            //是否已经停止运行
            std::vector<CommitResult> results(tasks.size(), {false, RefuseReason::ThreadPoolHasStoppedRunning});
            if (stopCommitState.load(std::memory_order_relaxed)){
                return results;
            }
            int leftSlot = QueueMaxSize -queueTask.size_approx();
            auto success = queueTask.enqueue_bulk(tasks.begin(), leftSlot);
            if (success){
                for (auto & result : results) {
                    result.reason = RefuseReason::NoRefuse;
                    result.isSuccess = true;
                }
                tryAddThread(); //尝试添加线程
            }else{
                for (auto & result : results) {
                    result.reason = RefuseReason::MemoryNotEnough;
                }
            }
            return results;
        }

        /* 提交多个任务 */
        std::vector<CommitResult> commit_executors(std::initializer_list<std::shared_ptr<Executor>> tasks){
            //懒加载 初始化
            std::call_once(initialFlag, [this]{
                initialize();
            });
            //是否已经停止运行
            std::vector<CommitResult> results(tasks.size(), {false, RefuseReason::ThreadPoolHasStoppedRunning});
            if (stopCommitState.load(std::memory_order_relaxed)){
                return results;
            }
            int leftSlot = QueueMaxSize -queueTask.size_approx();
            auto success = queueTask.enqueue_bulk(tasks.begin(), leftSlot);
            if (success){
                for (auto & result : results) {
                    result.reason = RefuseReason::NoRefuse;
                    result.isSuccess = true;
                }
                tryAddThread(); //尝试添加线程
            }else{
                for (auto & result : results) {
                    result.reason = RefuseReason::MemoryNotEnough;
                }
            }
            return results;
        }

        CommitResult commit_executor(const std::shared_ptr<Executor>& task){
            //懒加载 初始化
            std::call_once(initialFlag, [this]{
                initialize();
            });
            //返回值
            CommitResult result{false,RefuseReason::NoRefuse };
            //是否已经停止运行
            if (stopCommitState.load(std::memory_order_relaxed)){
                result.reason = RefuseReason::ThreadPoolHasStoppedRunning;
                return result;
            }
            {
                if (queueTask.size_approx() < QueueMaxSize){
                    //复制构造
                    if(queueTask.enqueue(task)) {
                        result.isSuccess = true;

                        tryAddThread(); //尝试添加线程
                    }else{
                        //内存分配失败
                        result.reason = RefuseReason::MemoryNotEnough;
                    }
                }else{
                    result.reason = RefuseReason::TaskQueueFulled;
                }
            }
            return result;
        }

        //按照关闭策略 关闭 线程池
        std::queue<std::shared_ptr<Executor>> close(){
            std::queue<std::shared_ptr<Executor>> result;
            stopCommitState.store(true, std::memory_order_release);
            if (closeStrategy == ThreadPoolCloseStrategy::WaitAllTaskFinish){
                bool isFinishAllTask = false;
                while (!isFinishAllTask){
                    if(queueTask.size_approx() == 0){
                        isFinishAllTask = true;
                    }else{
                        std::this_thread::sleep_for(10ms);
                    }
                }
            }
            runState.store(false, std::memory_order_release);
            if (closeStrategy == ThreadPoolCloseStrategy::ReturnTaskAndClose){
                std::shared_ptr<Executor> ex;
                while (queueTask.try_dequeue(ex)){
                    result.emplace(ex);
                }
            }
            if (closeStrategy == ThreadPoolCloseStrategy::DiscardAllTasks){
                std::shared_ptr<Executor> ex;
                while (queueTask.try_dequeue(ex)){
                    ex->discardState = true; //被丢了
                    result.emplace(ex);
                }
            }
            // 唤醒所有线程执行 毁灭操作
            for (std::shared_ptr<ConcurrentWorker> &wr: workers) {
                //等待工作线程死掉
                if (wr->workman != nullptr){
                    if (wr->workman->joinable()) wr->workman->join();
                }
            }
            isTerminated.store(true, std::memory_order_release);
            return result;;
        }

        ~ConcurrentThreadPool(){
            if (!isStart){
                //就没有启动过线程池
            }else{
                //是否调用过 close 方法！
                if(!isTerminated.load(std::memory_order_acquire)) close();
            }
        }
    };

}

#endif //MUSE_CONCURRENT_QUEUE_POOL_HPP
