//
// Created by remix on 23-7-26.
//

#include "timer_tree.hpp"

namespace muse::timer{

    TimeNodeBase::TimeNodeBase(uint64_t Id, time_t exp):ID(Id),expire(exp){

    }

    uint64_t TimeNodeBase::getID() const{
        return ID;
    }
    time_t TimeNodeBase::getExpire() const{
        return expire;
    }


    TimeNode::TimeNode(uint64_t Id, CallBack cb, time_t exp)
    :TimeNodeBase(Id, exp), callBack(std::move(cb)){

    }

    uint64_t TimerTree::GenTimeTaskID(){
        std::unique_lock<std::mutex> lock(mtx);
        return  ++initID;
    }

    /* 获得当前时间 */
    time_t TimerTree::GetTick(){
        std::chrono::time_point<std::chrono::system_clock> tp =
                std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now()
                );
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    }

    bool TimerTree::clearTimeout(TimeNodeBase &nodeBase){
        auto it = nodes.find(nodeBase);
        if (it != nodes.end()){
            nodes.erase(it);
            return true;
        }
        return false;
    }

    time_t TimerTree::checkTimeout(){
        if (nodes.empty()) return -1;//没有定时器时间
        time_t diff = nodes.begin()->getExpire() - TimerTree::GetTick();
        return diff > 0? diff:0;
    }


    bool TimerTree::runTask(){
        if (!nodes.empty()){
            //迭代器，但是加锁了
            auto it = nodes.begin();
            time_t diff = it->getExpire() - TimerTree::GetTick();
            if (diff <= 0){
                it->callBack(); //调用 会不会有异常啊
                nodes.erase(it);
                return true;
            }
        }
        return false;
    }

    void TimerTree::runTaskLoop() {
        auto current = TimerTree::GetTick();
        auto start = nodes.begin();
        while (start != nodes.end()){
            time_t diff = start->getExpire() - current;
            if (diff <= 0){
                start->callBack();
                start = nodes.erase(start);
            }else{
                break;
            }
        }
    }
}