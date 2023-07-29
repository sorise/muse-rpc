//
// Created by remix on 23-7-24.
//
#include "conf.h"

namespace muse::pool{
    extern std::chrono::milliseconds GetTick(){
        std::chrono::time_point<std::chrono::system_clock> tp =
                std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
    }
}