//
// Created by remix on 23-7-28.
//

#include "middleware_channel.hpp"
namespace muse::rpc{

    //饿汉式，线程安全
    MiddlewareChannel*  MiddlewareChannel::instance = MiddlewareChannel::GetInstance();

    std::mutex MiddlewareChannel::mtx = std::mutex();

    /* 中间件管道，数据怎么进入就怎么出去 */
    MiddlewareChannel * MiddlewareChannel::GetInstance() {
        if (MiddlewareChannel::instance == nullptr){
            std::lock_guard<std::mutex> lock(MiddlewareChannel::mtx);
            if (MiddlewareChannel::instance == nullptr){
                MiddlewareChannel::instance = new MiddlewareChannel();
                static GC gc;
            }
        }
        return instance;
    }

    std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>>
    MiddlewareChannel::In(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) {
        std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>> temp(data, data_size, _memory_pool);
        for (auto &service: services) {
            temp = service->In(std::get<0>(temp),std::get<1>(temp),std::get<2>(temp));
        }
        return temp;
    }

    std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>>
    MiddlewareChannel::Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) {
        std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>> temp(data, data_size, _memory_pool);
        //反过来调用
        auto it_r_begin = services.rbegin();
        auto it_r_end = services.rend();
        for (; it_r_begin != it_r_end ; it_r_begin++) {
            temp = (*it_r_begin)->Out(std::get<0>(temp),std::get<1>(temp),std::get<2>(temp));
        }
        return temp;
    }

    std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>>
    MiddlewareChannel::ClientIn(std::shared_ptr<char[]> data, size_t data_size,
                               std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) {
        std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>> temp(data, data_size, _memory_pool);
        if (services.size() > 1){
            for (int i = 0; i < services.size() - 1; ++i) {
                temp = services[i]->In(std::get<0>(temp),std::get<1>(temp),std::get<2>(temp));
            }
        }
        return temp;
    }

    std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>>
    MiddlewareChannel::ClientOut(std::shared_ptr<char[]> data, size_t data_size,
                                 std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool) {
        std::tuple<std::shared_ptr<char[]>, size_t, std::shared_ptr<std::pmr::synchronized_pool_resource>> temp(data, data_size, _memory_pool);
        if (services.size() > 1){
            for (int i = services.size() - 2; i >= 0 ; i--) {
                temp = services[i]->Out(std::get<0>(temp),std::get<1>(temp),std::get<2>(temp));
            }
        }
        return temp;
    }


}