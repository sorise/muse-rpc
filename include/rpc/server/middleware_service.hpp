//
// Created by remix on 23-7-28.
//
#ifndef MUSE_RPC_MIDDLEWARE_SERVICE_HPP
#define MUSE_RPC_MIDDLEWARE_SERVICE_HPP
#include <memory>
#include <memory_resource>
#include <iostream>

namespace muse::rpc{
    class middleware_service {
    public:
        middleware_service() = default;
        //数据输入 解压、解密
        virtual std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        In(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) = 0;

        //数据输出 压缩、加密
        virtual std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) = 0;

        virtual ~middleware_service() = default;
    };

}

#endif //MUSE_RPC_MIDDLEWARE_SERVICE_HPP
