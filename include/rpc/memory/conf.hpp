//
// Created by remix on 23-7-29.
//

#ifndef MUSE_RPC_MEMORY_CONF_HPP
#define MUSE_RPC_MEMORY_CONF_HPP
#include "rpc/server/middleware_channel.hpp"
#include <memory>

namespace muse::rpc{
    extern std::pmr::synchronized_pool_resource* make_memory_pool();

    extern std::shared_ptr<std::pmr::synchronized_pool_resource> MemoryPoolSingleton();

}

#endif //MUSE_RPC_MEMORY_CONF_HPP
