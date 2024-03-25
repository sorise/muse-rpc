//
// Created by remix on 23-7-26.
//

#ifndef MUSE_RPC_TIMER_TREE_HPP
#define MUSE_RPC_TIMER_TREE_HPP
#include <iostream>
#include <chrono>
#include <limits>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <utility>
#include "spdlog/spdlog.h"

/* 基于红黑树的非线程安全的毫秒级定时器 */
namespace muse::timer{
    class TimeNodeBase{
    private:
        uint64_t ID; //标识ID
        time_t expire;  //啥时候过期
    public:
        TimeNodeBase(uint64_t Id, time_t exp);
        uint64_t getID() const;
        time_t getExpire() const;
    };

    static bool operator < (const TimeNodeBase &me, const TimeNodeBase &other){
        if (me.getExpire() < other.getExpire()){
            return true;
        }else if(me.getExpire() > other.getExpire()){
            return false;
        }
        return me.getID() < other.getID(); //前插入的在前面 后插入的放在后面
    }

    class TimeNode: public TimeNodeBase{
    public:
        using CallBack = std::function<void()>;
        CallBack callBack; //毁掉函数
        TimeNode(uint64_t Id,CallBack cb, time_t exp);
    };

    class TimerTree {
    private:
        std::mutex mtx;
        uint64_t initID {0};
        /* 存放执行节点 */
        std::set<TimeNode, std::less<>> nodes;
        /* 获得当前时间 */
        static time_t GetTick();
        /* 获得时间唯一ID */
        uint64_t GenTimeTaskID();
    public:
        TimerTree() = default;

        //添加到树上  
        template<class F, class ...Args >
        TimeNodeBase setTimeout(long long milliseconds, F && f, Args&... args){
            TimeNode::CallBack callBack = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            TimeNode tNode(GenTimeTaskID() ,callBack, GetTick() + milliseconds);
            nodes.insert(tNode);
            return static_cast<TimeNodeBase>(tNode);
        }

        //添加到树上普通 函数指针  + 引用
        template<typename F,typename R, typename ...Args>
        TimeNodeBase setTimeout(long long milliseconds, F&& f, R& r,  Args&&... args){
            TimeNode::CallBack callBack = std::bind(std::forward<F>(f) , std::ref(r) ,std::forward<Args>(args)...);
            TimeNode tNode(GenTimeTaskID() ,callBack, GetTick() + milliseconds);
            nodes.insert(tNode);
            return static_cast<TimeNodeBase>(tNode);
        }

        //添加到树上 函数指针 + 指针
        template<typename F, typename R, typename ...Args>
        TimeNodeBase setTimeout(long long milliseconds, F&& f, R* r,  Args&&... args){
            TimeNode::CallBack callBack = std::bind(std::forward<F>(f), r ,std::forward<Args>(args)...);
            TimeNode tNode(GenTimeTaskID() ,callBack, GetTick() + milliseconds);
            nodes.insert(tNode);
            return static_cast<TimeNodeBase>(tNode);
        }

        /* 取消任务 */
        bool clearTimeout(TimeNodeBase &nodeBase);

        /* 返回还有多久时间才有时间进行触发 */
        time_t checkTimeout();

        /* 执行任务 */
        bool runTask();
    };
}


#endif //MUSE_RPC_TIMER_TREE_HPP
