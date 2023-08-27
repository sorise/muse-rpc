//
// Created by remix on 23-7-28.
//

#ifndef MUSE_RPC_ROUTE_SERVICE_HPP
#define MUSE_RPC_ROUTE_SERVICE_HPP
#include "middleware_service.hpp"
#include "synchronous_registry.hpp"
#include "registry.hpp"
#include "../logger/conf.hpp"

namespace muse::rpc{
    //路由服务，用于方法的定位
    class RouteService: public middleware_service{

    private:
        std::string prefix_name {"@context/"};
        ByteSequence sq  { muse::serializer::getByteSequence()};
        std::shared_ptr<SynchronousRegistry> concurrent_registry;
        std::shared_ptr<Registry> registry;


    public:
        RouteService(std::shared_ptr<Registry> _registry, std::shared_ptr<SynchronousRegistry> _concurrent_registry);

        //数据输入 解压、解密
        std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        In(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override;

        //数据输出 压缩、加密
        std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) override;

        ~RouteService() override = default;
    };
}

#endif //MUSE_RPC_ROUTE_SERVICE_HPP
