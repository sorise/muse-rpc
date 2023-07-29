//
// Created by remix on 23-7-24.
//
#include "executor.h"

namespace muse::pool{
    Executor::Executor(Task inTask)
            :task(std::move(inTask)),
             finishState(false),
             discardState(false){

    }

    Executor::Executor(Executor &&other) noexcept
            :task(std::move(other.task)),
             finishState(other.finishState),
             discardState(other.discardState){

    }
}