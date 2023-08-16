//
// Created by remix on 23-8-16.
//
#include "global_entry.hpp"

namespace muse::rpc{
    std::unique_ptr<GlobalEntry::Connections_Queue> GlobalEntry::con_queue = std::make_unique<GlobalEntry::Connections_Queue>();

    std::unique_ptr<TimerTree> GlobalEntry::timerTree = std::make_unique<TimerTree>();

    std::mutex GlobalEntry::mtx = std::mutex();
}