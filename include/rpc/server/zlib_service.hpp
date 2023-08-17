//
// Created by remix on 23-7-30.
//

#ifndef MUSE_RPC_ZLIB_SERVICE_HPP
#define MUSE_RPC_ZLIB_SERVICE_HPP
#include "../logger/conf.hpp"
#include <algorithm>
#include "middleware_service.hpp"
#include "../protocol/protocol.hpp"
#include <zlib.h>

namespace muse::rpc{
    class ZlibService: public middleware_service {
    private:
        Protocol protocol;
    public:
        ZlibService() = default;
        ~ZlibService() override = default;

        //数据输入 解压、解密
        std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        In(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override;

        //数据输出 压缩、加密
        std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override;

    };
}


#endif //MUSE_RPC_ZLIB_SERVICE_HPP
