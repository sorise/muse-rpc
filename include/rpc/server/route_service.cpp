//
// Created by remix on 23-7-28.
//

#include "route_service.hpp"

namespace muse::rpc{

    std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>>
    RouteService::In(std::shared_ptr<char[]> data, size_t data_size,
                     std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) {
        muse::serializer::BinarySerializer serializer;
        //效率不高，唉.... 需要优化，需要一个无复制序列化器
        serializer.write(data.get(),data_size);
        std::string request_name;
        try {
            serializer.output(request_name); //获得方法的名称
            bool isExist = registry->check(request_name);
            bool isExistConcurrent = concurrent_registry->check(request_name);

            if (!(isExist | isExistConcurrent)){
                //方法不存在
                RpcResponseHeader header;
                header.setOkState(false);
                header.setReason(RpcFailureReason::MethodNotExist);
                serializer.clear();
                serializer.input(header);
            }else{
                //存在方法
                if (isExist){
                    registry->runEnsured(request_name, &serializer);
                    //返回值 存放在 serializer 中！
                }else{
                    concurrent_registry->runEnsured(request_name, &serializer);
                }
            }
        }
        catch (...)
        {
            //读取方法名称错误
            serializer.clear();
            RpcResponseHeader header;
            header.setOkState(false);
            header.setReason(RpcFailureReason::MethodExecutionError);
            serializer.clear();
            serializer.input(header);
        }
        auto count = serializer.byteCount();
        auto ptr = _memory_pool->allocate(count);
        std::shared_ptr<char[]> rdt((char*)ptr, [_memory_pool, total = count](char *ptr){
            _memory_pool->deallocate(ptr, total);
        });
        std::memcpy(rdt.get(),serializer.getBinaryStream(), serializer.byteCount());
        return std::make_tuple(rdt, serializer.byteCount(), _memory_pool);
    }

    std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>>
    RouteService::Out(std::shared_ptr<char[]> data, size_t data_size,
                      std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) {
        return std::make_tuple(data, data_size, _memory_pool);
    }


    RouteService::RouteService(std::shared_ptr<Registry> _registry, std::shared_ptr<SynchronousRegistry> _concurrent_registry)
    :concurrent_registry(std::move(_concurrent_registry)),registry(std::move(_registry))
    {

    }
}