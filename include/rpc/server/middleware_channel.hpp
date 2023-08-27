//
// Created by remix on 23-7-28.
//

#ifndef MUSE_RPC_MIDDLEWARE_CHANNEL_HPP
#define MUSE_RPC_MIDDLEWARE_CHANNEL_HPP
#include "middleware_service.hpp"
#include <atomic>
#include <mutex>
#include <algorithm>

namespace muse::rpc{
    template <class T, typename ...Argc>
    T* Singleton(Argc&& ...argc) {
        static T *instance = new T(argc...);
        return instance;
    }//问题在于没办法 回收 内存，调用析构函数。

    // 饿汉式
    /* 中间件管道，数据怎么进入就怎么出去 */
    class MiddlewareChannel{
    public:
        static MiddlewareChannel* GetInstance();
        //已经注册的服务
        std::vector<std::shared_ptr<middleware_service>> services;
    private:
        MiddlewareChannel() = default;
        MiddlewareChannel(const MiddlewareChannel& other) = default;
        MiddlewareChannel& operator=(const MiddlewareChannel&) = default;
        ~MiddlewareChannel() = default;
        static MiddlewareChannel* instance ;
        static std::mutex mtx;
        /* 用于销毁对象 */
        class GC{
        public:
            GC() = default;
            ~GC(){
                if (instance != nullptr){
                    delete instance;
                    instance = nullptr;
                }
            }
        };
    public:
        template<typename M, typename ...Argc>
        typename std::enable_if<std::is_base_of<middleware_service, M>::value, void>::type
        Register(Argc&& ...argc){
            M * mp = Singleton<M>(argc...);
            auto it = std::find_if(services.begin(), services.end(), [=](std::shared_ptr<middleware_service> &ptr){
                return  ptr.get() == mp;
            });
            if (it == services.end()){
                services.emplace_back(std::shared_ptr<M>(mp));
            }
        }

        template<typename M, typename ...Argc>
        static typename std::enable_if<std::is_base_of<middleware_service, M>::value, void>::type
        configure(Argc&& ...argc) {
            MiddlewareChannel::GetInstance()->Register<M>(argc...);
        }

        //数据输入 解压、解密
        std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        In(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool);

        //数据输出 压缩、加密
        virtual std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        Out(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool);

        //数据输入 解压、解密
        std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        ClientIn(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool);

        //数据输入 解压、解密
        std::tuple<std::shared_ptr<char[]>, size_t , std::shared_ptr<std::pmr::synchronized_pool_resource>>
        ClientOut(std::shared_ptr<char[]> data, size_t data_size, std::shared_ptr<std::pmr::synchronized_pool_resource> _memory_pool);

    };
}



#endif //MUSE_RPC_MIDDLEWARE_CHANNEL_HPP
