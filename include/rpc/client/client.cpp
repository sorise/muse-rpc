//
// Created by remix on 23-7-27.
//

#include "client.hpp"

namespace muse::rpc{
    Client::Client(const char *_ip_address, const uint16_t &_port, std::shared_ptr<std::pmr::synchronized_pool_resource> _pool)
    : invoker(_ip_address, _port), factory(std::move(_pool)){

    }

    void Client::Bind(uint16_t _local_port) {
        invoker.Bind(_local_port);
    }
};
