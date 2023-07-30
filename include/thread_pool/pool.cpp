//
// Created by remix on 23-7-28.
//
#include "pool.hpp"

#include <memory>

namespace muse::pool{
    //核心线程运行逻辑
    void ThreadPool::coreRun(unsigned int threadIndex){
        //一直循环
        while (true){
            //获得待执行任务
            std::shared_ptr<Executor> executorPtr;
            {
                std::unique_lock<std::mutex> unique{ queueMute };
                // 阻塞等待, 等待任务队列非空 或者 线程池已经停止！
                condition.wait(unique, [this]{
                    return !queueTask.empty() ||!runState.load(std::memory_order_relaxed);
                });
                if (!runState.load(std::memory_order_relaxed)){
                    // 如果线程池已经停止了 直接返回
                    return ;
                }
                // 获得执行任务
                executorPtr = std::move(queueTask.front());
                queueTask.pop(); //出队列
                vacantThreadsCount.fetch_sub(1,std::memory_order_release); //有一个在工作了
            }
            //任务没有被执行
            if (!executorPtr->finishState){
                //执行任务，防止异常导致 线程池崩溃！
                try {
                    //正常执行完成状态
                    executorPtr->task();
                }catch(...){
                    //如果执行失败
                    executorPtr->haveException = true;
                }
                executorPtr->finishState = true;
            }
            vacantThreadsCount.fetch_add(1, std::memory_order_release); //又有空闲线程了
        }
    }

    //动态线程运行逻辑
    void ThreadPool::dynamicRun(unsigned int dynamicThreadIndex){
        while (true){
            //获得待执行任务
            std::shared_ptr<Executor> executorPtr;
            {
                std::unique_lock<std::mutex> unique{ queueMute };
                // 阻塞等待, 等待任务队列非空 或者 线程池已经停止！
                condition.wait(unique, [this, dynamicThreadIndex]{
                    return !queueTask.empty()
                           ||!runState.load(std::memory_order_relaxed)
                           ||!workers[dynamicThreadIndex]->isRunning.load(std::memory_order_acquire);
                });
                if (!runState.load(std::memory_order_relaxed)){
                    return; // 如果线程池已经停止了 直接返回
                }
                if (!workers[dynamicThreadIndex]->isRunning.load(std::memory_order_acquire)){
                    return; // 当前线程需要关闭
                }
                executorPtr = std::move(queueTask.front()); // 获得执行任务
                queueTask.pop(); // 出队列
                vacantThreadsCount.fetch_sub(1,std::memory_order_release); //有一个在工作了
            }
            //任务没有被执行
            if (!executorPtr->finishState){
                //执行任务
                try {
                    //正常执行完成状态
                    executorPtr->task();
                    executorPtr->finishState = true;
                }catch(...){
                    executorPtr->finishState = true;
                    executorPtr->haveException = true;
                }
            }
            vacantThreadsCount.fetch_add(1, std::memory_order_release); //又有空闲线程了
        }
    }

    /* 添加核心工作线程 */
    bool ThreadPool::addCoreThread(unsigned int count) noexcept{
        try{
            for(unsigned int i = 0; i < count; i++){
                //追加一个核心工作线程
                std::shared_ptr<std::thread> wr(new std::thread(&ThreadPool::coreRun, this, i));
                workers[i]->isRunning.store(true, std::memory_order_release);
                workers[i]->workman = wr;
            }
        }catch(const std::bad_alloc & ex){
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

    /* 添加动态线程 */
    bool ThreadPool::addDynamicThread() noexcept{
        {
            try{
                int i = cores;
                for (; i < MaxThreadCount; ++i) {
                    std::shared_ptr<Worker> worker = workers[i];
                    if(!worker->isRunning.load(std::memory_order_relaxed)){
                        // 找到添加位置
                        worker->isRunning.store(true, std::memory_order_relaxed);
                        //可能抛出异常
                        std::shared_ptr<std::thread> wr(new std::thread(&ThreadPool::dynamicRun, this, i));
                        workers[i]->workman = wr;
                        dynamicWorkerCount++;
                        return true;
                    }
                }
                //没有找到空闲位置
                return false;
            }catch(const std::exception& e){
                //需要一个日志系统
                return false;
            }catch(const char *error){
                std::cerr<< error << '\n';
                return false;
            }catch (...) {
                return false;
            }
        }
    }

    bool ThreadPool::killDynamicThread() noexcept{
        try {
            int i = cores;
            for (; i < MaxThreadCount; ++i) {
                std::shared_ptr<Worker> worker = workers[i];
                if(worker->isRunning.load(std::memory_order_relaxed)){
                    // 找到一个线程位置
                    worker->isRunning.store(false, std::memory_order_release);
                    condition.notify_all();
                    if(workers[i]->workman->joinable()) workers[i]->workman->join();  //等待线程死亡
                    dynamicWorkerCount--;
                    worker->workman = nullptr;
                    return true;
                }
            }
        }catch(const std::exception& e){
            //需要一个日志系统
            return false;
        }catch (...) {
            return false;
        }
        return false;
    }

    /* 初始化代码 */
    void ThreadPool::init(){
        //如果是弹性线程池
        try {
            if(ThreadPoolType::Flexible == Type){
                workers.resize(MaxThreadCount);
                for (int i = 0; i < MaxThreadCount; ++i) {
                    workers[i] = std::make_shared<Worker>();
                }
            }else if (ThreadPoolType::Fixed == Type){
                //如果是固定大小的线程池
                workers.resize(cores);
                for (int i = 0; i < cores; ++i) {
                    workers[i] = std::make_shared<Worker>();
                }
            }
        }catch(...){
            throw std::runtime_error("initialize the thread pool failed! memory not enough!");
        }
        //线程开始运行
        runState.store(true, std::memory_order_release);
        //添加核心工作线程
        addCoreThread(cores);
        //标记空闲数量
        vacantThreadsCount.store(cores);
        //判断是否启动管理线程
        if (Type == ThreadPoolType::Flexible && cores < MaxThreadCount ){
            //启动管理线程,实现动态增长
            try {
                managerThread = std::make_shared<std::thread> (&ThreadPool::managerThreadTask, this);
            }catch(...){
                throw std::runtime_error("The management thread failed to start due to insufficient memory!");
            }
        }
        isStart = true;
    }

    /* 只有线程池类型是 Flexible 才可以调用 */
    void ThreadPool::managerThreadTask(){
        //一旦 stopCommitState == true 那么线程池已经进入 开始关闭状态
        while(!stopCommitState.load(std::memory_order_acquire)){
            //运行频率
            std::this_thread::sleep_for(timeUnit);
            //获得空闲线程数量
            int vacant =  vacantThreadsCount.load(std::memory_order_relaxed);
            if (vacant > 2){
                //策略：如果存在空闲，且存在动态线程，那么干掉一个动态线程
                if (dynamicWorkerCount > 0){
                    killDynamicThread();
                }
                //策略：如果存在空闲，且不存在动态现存，不管！
            }else if(vacant <= 1){
                //策略：如果空闲数量 < 1 且 任务队列不为空 还可以增加线程，追加一个
                if (dynamicWorkerCount < MaxThreadCount - cores){
                    addDynamicThread();
                }
            }
        }
    }

    ThreadPool::ThreadPool(size_t coreThreadsCount, size_t _maxThreadCount, size_t _queueMaxSize, ThreadPoolType _type,  ThreadPoolCloseStrategy poolCloseStrategy, std::chrono::milliseconds unit)
    :cores(coreThreadsCount),
    closeStrategy(poolCloseStrategy),
    dynamicWorkerCount(0),
    MaxThreadCount(_maxThreadCount),
    QueueMaxSize(_queueMaxSize),
    Type(_type),
    timeUnit(unit)
    {
        if (coreThreadsCount > MaxThreadCount)
            throw std::runtime_error("(count <= MaxThreadCount)The number of core threads exceeds the maximum specified number of threads！");
        cores = (cores <= 0)?4:cores; //初始化线程数量
    }

    /* 提交多个任务 */
    std::vector<CommitResult> ThreadPool::commit_executors(const std::vector<std::shared_ptr<Executor>>& tasks ){
        //懒加载 初始化
        std::call_once(initFlag, [this]{
            init();
        });
        //是否已经停止运行
        std::vector<CommitResult> results(tasks.size(), {false, RefuseReason::ThreadPoolHasStoppedRunning});
        if (stopCommitState.load(std::memory_order_relaxed)){
            return results;
        }
        {
            const std::lock_guard<std::mutex> lock{ queueMute };
            int leftSlot = QueueMaxSize -queueTask.size();
            int i = 0;
            for (;i < leftSlot && i < tasks.size() ; ++i) {
                queueTask.emplace(tasks[i]);
                results[i].reason = RefuseReason::NoRefuse;
                results[i].isSuccess = true;
            }
            for (; i < results.size(); ++i) {
                results[i].reason = RefuseReason::TaskQueueFulled;
            }
            condition.notify_all();
        }
        return results;
    }

    std::vector<CommitResult> ThreadPool::commit_executors(std::initializer_list<std::shared_ptr<Executor>> tasks){
        //懒加载 初始化
        std::call_once(initFlag, [this]{
            init();
        });
        //是否已经停止运行
        std::vector<CommitResult> results(tasks.size(), {false, RefuseReason::ThreadPoolHasStoppedRunning});
        if (stopCommitState.load(std::memory_order_relaxed)){
            return results;
        }
        {
            const std::lock_guard<std::mutex> lock{ queueMute };
            int leftSlot = QueueMaxSize -queueTask.size();
            auto begin = tasks.begin();
            int i = 0;
            for (;i < leftSlot && i < tasks.size() ; ++i) {
                queueTask.emplace((*begin));
                begin++;
                results[i].reason = RefuseReason::NoRefuse;
                results[i].isSuccess = true;
            }
            for (; i < results.size(); ++i) {
                results[i].reason = RefuseReason::TaskQueueFulled;
            }
            condition.notify_all();
        }
        return results;
    }

    CommitResult ThreadPool::commit_executor(const std::shared_ptr<Executor>& task){
        //懒加载 初始化
        std::call_once(initFlag, [this]{
            init();
        });
        //返回值
        CommitResult result{false,RefuseReason::NoRefuse };
        //是否已经停止运行
        if (stopCommitState.load(std::memory_order_relaxed)){
            result.reason = RefuseReason::ThreadPoolHasStoppedRunning;
            return result;
        }
        {
            const std::lock_guard<std::mutex> lock{ queueMute };
            if (queueTask.size() < QueueMaxSize){
                result.isSuccess = true;
                //因为有的任务可能是线程池被丢弃的
                task->discardState = false;
                //复制构造
                queueTask.emplace(task);
                condition.notify_one();
            }else{
                result.reason = RefuseReason::TaskQueueFulled;
            }
        }
        return result;
    }

    size_t ThreadPool::taskSize() noexcept{
        std::lock_guard<std::mutex> lock(queueMute);
        return queueTask.size();
    }

    //按照关闭策略 关闭 线程池
    std::queue<std::shared_ptr<Executor>> ThreadPool::close(){
        std::queue<std::shared_ptr<Executor>> result;
        if (isTerminated.load(std::memory_order_relaxed)){
            //防止重复关闭
            return result;
        }
        //关闭管理线程
        stopCommitState.store(true, std::memory_order_release);
        //按照执行策略 继续关闭
        if (closeStrategy == ThreadPoolCloseStrategy::WaitAllTaskFinish){
            bool isFinishAllTask = false;
            while (!isFinishAllTask){
                {
                    std::lock_guard<std::mutex> lock(queueMute);
                    if(queueTask.empty()){
                        isFinishAllTask = true;
                    }else{
                        std::cout <<queueTask.size() << ": no finish" << std::endl;
                    }
                }
                std::this_thread::sleep_for(10ms);
            }
        }
        //管理线程 先死
        if (managerThread != nullptr && managerThread->joinable()){
            managerThread->join();
        }
        runState.store(false, std::memory_order_release);
        if (closeStrategy == ThreadPoolCloseStrategy::DiscardAllTasks){
            std::lock_guard<std::mutex> lock(queueMute);
            size_t count = queueTask.size();
            for (int i = 0; i < count ; ++i) {
                queueTask.front()->discardState = true;
                queueTask.pop();
            }
        }
        if (closeStrategy == ThreadPoolCloseStrategy::ReturnTaskAndClose){
            std::lock_guard<std::mutex> lock(queueMute);
            size_t count = queueTask.size();
            for (int i = 0; i < count ; ++i) {
                queueTask.front()->discardState = true;
                result.push(queueTask.front());
                queueTask.pop();
            }
        }
        condition.notify_all();
        // 唤醒所有线程执行 毁灭操作
        for (std::shared_ptr<Worker> &wr: workers) {
            //等待工作线程死掉
            if (wr->workman != nullptr){
                if (wr->workman->joinable()) wr->workman->join();
            }
        }
        //已经完成终结
        isTerminated.store(true, std::memory_order_release);
        return result;
    }

    ThreadPool::~ThreadPool(){
        if (!isStart){
            //就没有启动过线程池
        }else{
            //是否调用过 close 方法！
            if(!isTerminated.load(std::memory_order_acquire)) close();
        }
    }
}