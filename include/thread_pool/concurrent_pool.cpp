//
// Created by remix on 23-7-28.
//
#include "concurrent_pool.hpp"

namespace muse::pool{
    /* 核心线程工作流程 */
    void ConcurrentThreadPool::coreRun(unsigned int threadIndex){
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
                        return;
                    }
                }
                std::this_thread::yield();
            }
        }
    }

    /* 添加核心工作线程 */
    bool ConcurrentThreadPool::addCoreThread(unsigned int count){
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
    bool ConcurrentThreadPool::initialize(){
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
            throw ex;
        }catch(const std::exception & ex){
            throw ex;
        }catch(...){
            //观测失败
            return false;
        }
        return true;
    }

    bool ConcurrentThreadPool::tryAddThread(){
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

    ConcurrentThreadPool::ConcurrentThreadPool(size_t coreThreadsCount, size_t _queueMaxSize , ThreadPoolCloseStrategy poolCloseStrategy, std::chrono::milliseconds leisureUnit)
    :cores(coreThreadsCount),
    closeStrategy(poolCloseStrategy),
    leisureTimeUnit(leisureUnit),
    QueueMaxSize(_queueMaxSize),
    queueTask(_queueMaxSize)
    {
        cores = (cores <= 0)?4:cores; //初始化线程数量
    }

    /* 提交多个任务 */
    std::vector<CommitResult> ConcurrentThreadPool::commit_executors(const std::vector<std::shared_ptr<Executor>>& tasks ){
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
    std::vector<CommitResult> ConcurrentThreadPool::commit_executors(const std::list<std::shared_ptr<Executor>>& tasks ){
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
    std::vector<CommitResult> ConcurrentThreadPool::commit_executors(std::initializer_list<std::shared_ptr<Executor>> tasks){
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

    CommitResult ConcurrentThreadPool::commit_executor(const std::shared_ptr<Executor>& task){
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
    std::queue<std::shared_ptr<Executor>> ConcurrentThreadPool::close(){
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

    ConcurrentThreadPool::~ConcurrentThreadPool(){
        if (!isStart){
            //就没有启动过线程池
        }else{
            //是否调用过 close 方法！
            if(!isTerminated.load(std::memory_order_acquire)) close();
        }
    }
}