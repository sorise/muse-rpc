//
// Created by 14270 on 2023-06-22.
//
#ifndef MUSE_TIMER_WHEEL_TIMER_HPP
#define MUSE_TIMER_WHEEL_TIMER_HPP
#include <iostream>
#include <chrono>
#include <functional>
#include <exception>
#include <list>
#include <mutex>
#include <memory>
#include <thread>
#include <array>
#include <atomic>

namespace muse::timer{
    using namespace std::chrono_literals;

    class TimerWheel;

    //标识单个任务
    class TimerWheelTask{
        friend class TimerWheel;
    public:
        using CallBack = std::function<void()>;
        CallBack callBack; //回调函数
    private:
        std::chrono::milliseconds expire;  //啥时候过期
        std::chrono::milliseconds addDateTime;  //啥时候添加的
        std::atomic<bool> isCancel {false}; //是否停止执行
        bool isDuplicate; //是否重复执行
        unsigned long times; //已经执行了多少次
        std::chrono::milliseconds delayInterval; //需要循环执行
    public:
        uint64_t ID; //标识ID
        TimerWheelTask(CallBack cb, std::chrono::milliseconds exp);
        std::chrono::milliseconds getExpire() const;
        void cancel();
        bool getCancelState() const;
    };

    class TimerWheel{
    public:
        static constexpr const std::chrono::milliseconds tickGap = 5ms; //精度 5ms 最小间隔
        //提个醒
        static_assert(tickGap <= 10ms, "The minimum interval is less than the minimum accuracy requirement");
        //五层时间轮
        static const uint16_t levelCount = 5;
        //获得当前的时间，采取  system_clock
        static std::chrono::milliseconds GetTick();
    private:
        struct Bucket{
            //放在每个桶里面
            uint64_t initID {0};
            //锁
            std::mutex mtx;
            //任务队列, 但存储的是指针
            std::list<std::shared_ptr<TimerWheelTask>> tasks;
            //唯一表示一个任务
            uint64_t genTaskID();
        };
        //存储开始运行时间
        std::chrono::milliseconds startDataTime;
        //完成初始化
        std::atomic<uint32_t> tick = 0;

        static const uint32_t LevelOneSize = 256;
        static const uint32_t LevelTwoSize = 64;
        static const uint32_t LevelThreeSize = 64;
        static const uint32_t LevelFourSize = 64;
        static const uint32_t LevelFiveSize = 64;

        std::array<Bucket, LevelOneSize>   levelOne;  //第一层
        std::array<Bucket, LevelTwoSize>   levelTwo;   //第二层
        std::array<Bucket, LevelThreeSize> levelThree; //第三层
        std::array<Bucket, LevelFourSize>  levelFour;  //第四层
        std::array<Bucket, LevelFiveSize>  levelFive;  //第五层

        //用于取 余 操作
        uint32_t checkOne   = 0b00000000000000000000000011111111;
        uint32_t checkTwo   = 0b00000000000000000011111100000000;
        uint32_t checkThree = 0b00000000000011111100000000000000;
        uint32_t checkFour  = 0b00000011111100000000000000000000;
        uint32_t checkFive  = 0b11111100000000000000000000000000;

        bool state = true;
        //把一个 task 丢到任务里面去
        void setTimerWheelTask(std::chrono::milliseconds delay, std::shared_ptr<TimerWheelTask>& task);
    public:
        TimerWheel();
        ~TimerWheel();

        //禁止拷贝 赋值拷贝 移动 移动赋值
        TimerWheel(const TimerWheel& other) = delete;
        TimerWheel(TimerWheel&& other) = delete;
        TimerWheel& operator=(const TimerWheel&other) = delete;
        TimerWheel& operator=(TimerWheel&&) = delete;

        template<class F, class ...Args >
        std::shared_ptr<TimerWheelTask> setTimeout(const std::chrono::milliseconds& delay, F && f, Args&&... args){
            if (delay.count() >= 21474836480){
                //超过限制 超时时间不能大于等于 2^32 毫秒
                throw std::logic_error("[001] Delay time out of range!");
            }
            if (delay.count() <= 0){
                //超时时间设置不规范
                throw std::logic_error("[002] The Delay parameter passed in is not standardized!");
            }
            try {
                //绑定一个回调函数
                auto callBack = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
                // 创建一个超时任务
                std::shared_ptr<TimerWheelTask> task(new TimerWheelTask(callBack, GetTick() + delay));
                setTimerWheelTask(delay, task);
                return task;
            }catch(std::bad_alloc &allocExp){
                return nullptr;
            }
        }

        template<class F, class ...Args >
        std::shared_ptr<TimerWheelTask> setInterval(const std::chrono::milliseconds& delay, F && f, Args&&... args){
            if (delay.count() >= 21474836480){
                //超过限制 超时时间不能大于等于 2^32 毫秒
                throw std::logic_error("[001] Delay time out of range!");
            }
            if (delay.count() <= 0){
                //超时时间设置不规范
                throw std::logic_error("[002] The Delay parameter passed in is not standardized!");
            }
            if (delay <= 10ms){
                //超时时间设置不规范, 会导致加锁失败。
                //std::recursive_mutex
                throw std::logic_error("[003] The minimum interval is less than the minimum accuracy requirement");
            }
            try {
                //绑定一个回调函数
                auto callBack = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
                // 创建一个超时任务
                std::shared_ptr<TimerWheelTask> task(new TimerWheelTask(callBack, GetTick() + delay));
                task->isDuplicate = true;
                task->delayInterval = delay;
                task->addDateTime = GetTick();
                setTimerWheelTask(delay, task);
                return task;
            }catch(std::bad_alloc &allocExp){
                return nullptr;
            }
        }

        static void clearTimeout(std::shared_ptr<muse::timer::TimerWheelTask>& task );

        static void clearInterval(std::shared_ptr<muse::timer::TimerWheelTask>& task );
    };
}
#endif //MUSE_TIMER_WHEEL_TIMER_HPP