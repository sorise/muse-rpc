//
// Created by remix on 23-8-13.
//
#include "global_id.hpp"

namespace muse::rpc{

    extern std::chrono::microseconds GetNowTickMicroseconds()
    {
        std::chrono::time_point<std::chrono::system_clock> tp =
                std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
    }

    uint64_t GlobalMicrosecondsId(){
        static std::atomic<uint64_t> tick = GetNowTickMicroseconds().count();
        tick.fetch_add(1, std::memory_order_release);
        return tick.load(std::memory_order_relaxed);
    }
}