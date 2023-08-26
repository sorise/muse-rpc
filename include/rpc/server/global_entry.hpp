// Created by remix on 23-8-16.


#ifndef MUSE_RPC_GLOBAL_ENTRY_HPP
#define MUSE_RPC_GLOBAL_ENTRY_HPP
#include <memory>
#include <mutex>
#include "timer/timer_tree.hpp"
#include "peer.hpp"
#include "reqeust.hpp"

using namespace muse::timer;

namespace muse::rpc{
    class GlobalEntry {
    public:
        using Connections_Queue = std::map<Peer, SocketConnection, std::less<>>;
        using Active_Message_Queue = std::set<Servlet, std::less<>>;

        //定时器
        static std::unique_ptr <TimerTree> timerTree;

        static std::unique_ptr <Connections_Queue> con_queue;            // 最近建立的链接

        static std::mutex mtx;

        static std::mutex mtx_active_queue;

        static std::unique_ptr<Active_Message_Queue> active_queue;     //服务器主动发起的请求
    };
}

#endif //MUSE_RPC_GLOBAL_ENTRY_HPP
