//
// Created by remix on 23-7-25.
//
#include "timer_wheel.hpp"
namespace muse::timer{

    TimerWheelTask::TimerWheelTask(CallBack cb, std::chrono::milliseconds exp):
    ID(0),
    expire(exp),
    callBack(std::move(cb)),
    isDuplicate(false),
    addDateTime(0ms),
    times(0),
    delayInterval(0ms){

    }

    std::chrono::milliseconds TimerWheelTask::getExpire() const { return expire; }

    void TimerWheelTask::cancel() { isCancel.store(true, std::memory_order_release); }

    bool TimerWheelTask::getCancelState() const { return isCancel.load(std::memory_order_relaxed);}

    std::chrono::milliseconds TimerWheel::GetTick(){
        std::chrono::time_point<std::chrono::system_clock> tp =
                std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
    }

    uint64_t TimerWheel::Bucket::genTaskID() {
        return  ++initID;
    }

    void TimerWheel::setTimerWheelTask(std::chrono::milliseconds delay, std::shared_ptr<TimerWheelTask>& task){
        task->ID = delay.count();
        // 延迟多少个 10ms 有待考究
        auto Gaps = delay / tickGap;
        // 当前的 tick 移动了多少次
        auto tickPlace = tick.load();
        // 最近触发 在第一层
        if (Gaps <= checkOne){
            // 获取当前 第一层时间轮的时针位置
            auto levelOnePointer = tickPlace & checkOne;
            // 距离
            auto distance = Gaps & checkOne;
            // 应该存放延迟任务的bucket位置
            auto bucket =(levelOnePointer + distance) & (LevelOneSize - 1);
            {
                // 加锁
                std::unique_lock<std::mutex> lock(levelOne[bucket].mtx);
                // 任务获得一个 ID
                task->ID = levelOne[bucket].genTaskID();
                // 存放任务
                levelOne[bucket].tasks.push_back(task);
            }
        }else if(Gaps <= checkTwo){
            // 应该存放延迟任务的bucket位置
            auto bucket = ((tickPlace + Gaps) & checkTwo) >> 8;
            {
                // 加锁
                std::unique_lock<std::mutex> lock(levelTwo[bucket].mtx);
                // 任务获得一个 ID
                task->ID = levelTwo[bucket].genTaskID();
                // 存放任务
                levelTwo[bucket].tasks.push_back(task);
            }
        }else if(Gaps <= checkThree){
            auto bucket = ((tickPlace + Gaps) & checkTwo) >> 8;
            {
                // 加锁
                std::unique_lock<std::mutex> lock(levelThree[bucket].mtx);
                // 任务获得一个 ID
                task->ID = levelThree[bucket].genTaskID();
                // 存放任务
                levelThree[bucket].tasks.push_back(task);
            }
        }else if(Gaps <= checkFour){
            // 获取当前 第四层时间轮的时针位置
            auto bucket = ((tickPlace + Gaps) & checkTwo) >> 20;
            {
                // 加锁
                std::unique_lock<std::mutex> lock(levelFour[bucket].mtx);
                // 任务获得一个 ID
                task->ID = levelFour[bucket].genTaskID();
                // 存放任务
                levelFour[bucket].tasks.push_back(task);
            }
        }else{
            auto bucket = ((tickPlace + Gaps) & checkTwo) >> 26;
            {
                // 加锁
                std::unique_lock<std::mutex> lock(levelFive[bucket].mtx);
                // 任务获得一个 ID
                task->ID = levelFive[bucket].genTaskID();
                // 存放任务
                levelFive[bucket].tasks.push_back(task);
            }
        }
    }

    TimerWheel::TimerWheel():startDataTime(TimerWheel::GetTick()){
        std::thread th([this](){
            while (state){
                //获得tick的位置
                uint32_t tickPlace = tick.load(std::memory_order::memory_order_relaxed);
                //当前时间
                auto nowTick = muse::timer::TimerWheel::GetTick();
                //第一层时间轮 时针所在位置
                uint32_t levelOneIndex = tickPlace & checkOne;
                //判断第二层 需要降低加锁频率，只有 二层指针下一个tick需要移动的时候才需要加锁
                uint32_t levelTwoIndex = (tickPlace & checkTwo) >> 8;
                //判断下一个 tick 第二层时针指向何处
                uint32_t levelTwoNext = ((tickPlace + 1) & checkTwo) >> 8;
                //到第三层 将第三层的任务放到第二层
                auto levelThreeIndex = (tickPlace & checkThree) >> 14;
                //判断下一个 tick 第三层时针指向何处
                auto levelThreeNext = ((tickPlace + 1) & checkThree) >> 14;
                //到第四层
                auto levelFourIndex = (tickPlace & checkFour) >> 20;
                //判断下一个 tick 第四层时针指向何处
                auto levelFourNext = ((tickPlace + 1) & checkFour) >> 20;
                //到第五层
                auto levelFiveIndex = (tickPlace & checkFive) >> 26;
                //判断下一个 tick 第三层时针指向何处
                auto levelFiveNext = ((tickPlace + 1) & checkFive) >> 26;

                // 先由上级向下级分发任务,再由第一层执行任务
                // 第五层向第四层分发任务 3.8836148 天 才会移动一次
                if ( ((levelFiveIndex + 1)&(LevelFiveSize - 1)) == levelFiveNext){
                    //防止反复对互斥量加锁
                    std::lock_guard<std::mutex> lock(levelFive[levelFiveNext].mtx);
                    if (!levelFive[levelFiveNext].tasks.empty()){
                        //有任务需要下方到下层时间轮
                        auto start = levelFive[levelFiveNext].tasks.begin();
                        // 开始下方
                        while (start != levelFive[levelFiveNext].tasks.end()){
                            if (!(*start)->getCancelState()){
                                //还有多少时间
                                auto delay = (*start)->getExpire() - nowTick;
                                //防止
                                if (delay < 0ms) delay = 0ms;
                                //换算成 Gap 数量
                                uint64_t Gaps = delay / tickGap;
                                //获取距离
                                auto distance = (Gaps & checkFour) >> 20 ;
                                //存放位置,不能使用下一层的当前指针，而是下一层的时针的下一个位置
                                auto bucket =  (levelFourNext + distance) & (LevelFourSize - 1);
                                //放在下一层的桶中
                                levelFour[bucket].tasks.push_back((*start));
                                //删除
                            }
                            start = levelFive[levelFiveNext].tasks.erase(start);
                        }
                    }
                }
                //第四层向第三层分发任务 1.45h  移动一次
                if (((levelFourIndex + 1)&(LevelFourSize - 1)) == levelFourNext){ //防止反复对互斥量加锁
                    std::lock_guard<std::mutex> lock(levelFour[levelFourNext].mtx);
                    if (!levelFour[levelFourNext].tasks.empty()){
                        //有任务需要下方到下层时间轮
                        auto start = levelFour[levelFourNext].tasks.begin();
                        while (start != levelFour[levelFourNext].tasks.end()){
                            if (!(*start)->getCancelState()){
                                //计算延迟
                                auto delay = (*start)->getExpire() - nowTick;
                                //防止错误
                                if (delay < 0ms) delay = 0ms;
                                //换算成 Gap 数量
                                uint64_t Gaps = delay / tickGap;
                                //获取距离
                                auto distance = (Gaps & checkThree) >> 14 ;
                                //存放位置
                                auto bucket =  (levelThreeNext + distance) & (LevelThreeSize - 1);
                                //放在桶中
                                levelThree[bucket].tasks.push_back((*start));
                            }
                            //删除
                            start = levelFour[levelFourNext].tasks.erase(start);
                        }
                    }
                }
                //第三层向第二层分发任务  81.92s 移动一次
                if (((levelThreeIndex + 1)&(LevelThreeSize - 1)) == levelThreeNext){
                    //防止反复对互斥量加锁
                    std::lock_guard<std::mutex> lock(levelThree[levelThreeNext].mtx);
                    if (!levelThree[levelThreeNext].tasks.empty()){
                        //迭代器 begin

                        auto start = levelThree[levelThreeNext].tasks.begin();
                        //开始迭代
                        while (start != levelThree[levelThreeNext].tasks.end()){
                            if (!(*start)->getCancelState()){
                                //判断还需要多久毫秒才轮到它执行
                                auto delay = (*start)->getExpire() - nowTick;
                                //必须要做的事情，不然有你好果子吃，经过长度一个小时的测试发行会出现负数的情况。
                                if (delay < 0ms) delay = 0ms;
                                //换算成 Gap 数量
                                uint64_t Gaps = delay / tickGap;
                                //防止提前问题, 还是防一手
                                Gaps = (Gaps >= checkTwo)? checkTwo:Gaps;
                                //找到距离
                                auto distance = (Gaps & checkTwo) >> 8;
                                //获得距离
                                auto bucket= (levelTwoNext + distance) & (LevelTwoSize - 1);
                                //放在桶中
                                levelTwo[bucket].tasks.push_back((*start));
                            }
                            //删除
                            start = levelThree[levelThreeNext].tasks.erase(start);
                        }
                    }
                }
                //第二层向第一层分发任务 1.28s分发一次
                if ( ((levelTwoIndex + 1)&(LevelTwoSize - 1)) == levelTwoNext){ //防止反复对互斥量加锁
                    //已经移动到新的bucket，需要分发任务
                    std::lock_guard<std::mutex> lock(levelTwo[levelTwoNext].mtx);
                    //将任务分发到下层时间论
                    if (!levelTwo[levelTwoNext].tasks.empty()){
                        //迭代器 begin
                        auto start = levelTwo[levelTwoNext].tasks.begin();
                        //迭代器 end
                        while (start != levelTwo[levelTwoNext].tasks.end()){
                            //如果当前任务还需要执行
                            if (!(*start)->getCancelState()){
                                //判断还需要多久才轮到它执行
                                auto delay = (*start)->getExpire() - nowTick;
                                //由于拖延导致 应该执行而还没有执行的任务
                                if (delay < 0ms) delay = 0ms;
                                //换算成 Gap数量
                                long long Gaps = delay / tickGap;
                                //由于网络对时，导致提早执行的任务
                                Gaps = Gaps >= checkOne? checkOne:Gaps;
                                //获得存放位置
                                auto bucket = (levelOneIndex + Gaps) & (LevelOneSize - 1);
                                //加入
                                levelOne[bucket].tasks.push_back((*start));
                            }
                            //移除
                            start = levelTwo[levelTwoNext].tasks.erase(start);
                        }
                    }
                }
                {//开始执行任务
                    //加锁
                    std::lock_guard<std::mutex> lock(levelOne[levelOneIndex].mtx);
                    //如果有任务的话
                    if (!levelOne[levelOneIndex].tasks.empty()){
                        auto start = levelOne[levelOneIndex].tasks.begin();
                        //遍历任务
                        while (start != levelOne[levelOneIndex].tasks.end()){
                            if (!(*start)->getCancelState()){
                                if (this->pool != nullptr){
                                    auto ex = muse::pool::make_executor((*start)->callBack);
                                    auto commit = pool->commit_executor(ex); //有可能会触发失败，因为任务队列已经满了
                                    if (!commit.isSuccess){
                                        (*start)->callBack();
                                    }
                                }else{
                                    (*start)->callBack();
                                }
                                //需要重复执行 需要防止死锁，好像这里需要递归加锁
                                if ((*start)->isDuplicate){
                                    //如果是重复执行
                                    std::shared_ptr<TimerWheelTask> task = (*start);
                                    task->times++;
                                    std::chrono::milliseconds nextTick = task->addDateTime + ((task->times + 1) * task->delayInterval);
                                    std::chrono::milliseconds delay = nextTick - TimerWheel::GetTick();
                                    task->expire = nextTick;
                                    if (delay < TimerWheel::tickGap * 2){ delay = TimerWheel::tickGap * 2;}
                                    //加入回去
                                    setTimerWheelTask(delay, task);
                                }
                            }
                            start = levelOne[levelOneIndex].tasks.erase(start);
                        }
                    }
                }
                //时钟 + 1
                this->tick.fetch_add(1, std::memory_order::memory_order_relaxed);
                //计算真实睡眠时间
                std::chrono::milliseconds sleepTime = (startDataTime + (tickPlace + 1) * tickGap) -  muse::timer::TimerWheel::GetTick();
                //start to run
                std::this_thread::sleep_for(sleepTime > 0ms?sleepTime:0ms);
            }
        });
        th.detach();
    }

    TimerWheel::~TimerWheel(){
        state = false;
    }

    void TimerWheel::clearTimeout(std::shared_ptr<muse::timer::TimerWheelTask>& task ) {
        task->cancel();
    }

    void TimerWheel::clearInterval(std::shared_ptr<muse::timer::TimerWheelTask>& task ){
        task->cancel();
        task->isDuplicate = false;
    }

    auto TimerWheel::inject_thread_pool(std::shared_ptr<muse::pool::ThreadPool> _pool) -> void {
        this->pool = _pool;
    }
}