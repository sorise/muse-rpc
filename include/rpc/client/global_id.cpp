//
// Created by remix on 23-8-13.
//
#include "global_id.hpp"

namespace muse::rpc{

    std::mutex global_mtx  = std::mutex();

    std::chrono::microseconds GetNowTickMicroseconds()
    {
        std::chrono::time_point<std::chrono::system_clock> tp =
                std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
    }

    std::chrono::nanoseconds GetNowTickNanometre()
    {
        std::chrono::time_point<std::chrono::system_clock> tp =
                std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch());
    }

    std::chrono::microseconds GetNowTickSeconds()
    {
        std::chrono::time_point<std::chrono::system_clock> tp =
                std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
    }

    uint64_t GlobalTransmitterId(){
        std::unique_lock<std::mutex> lock(global_mtx);
        static std::atomic<uint64_t> tick = 0;
        uint64_t nid = GetNowTickNanometre().count();
        if (nid <= tick){
            nid = tick +1;
        }
        tick = nid;
        return tick;
    }

    uint64_t GlobalSecondsId(){
        static std::atomic<uint64_t> tick = GetNowTickSeconds().count() / 1000000;
        tick.fetch_add(1, std::memory_order_release);
        return tick.load(std::memory_order_relaxed);
    }
}