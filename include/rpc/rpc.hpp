#ifndef MUSE_SERVER_RPC_HPP
#define MUSE_SERVER_RPC_HPP

#include <filesystem>
#include <exception>

#include "protocol/protocol.hpp"
#include "logger/conf.hpp"
#include "memory/conf.hpp"
#include "client/client.hpp"
#include "client/outcome.hpp"
#include "client/transmitter.hpp"
#include "client/transmitter_event.hpp"
#include "server/registry.hpp"
#include "server/synchronous_registry.hpp"
#include "server/reactor.hpp"
#include "server/sub_reactor.hpp"
#include "server/route_service.hpp"
#include "server/zlib_service.hpp"
#include "server/reactor_pool.hpp"
#include "server/middleware_channel.hpp"
#include "server/middleware_service.hpp"
#include "server/virtual_connection.hpp"

//异步方法，也就是当前方法可以同时被多个线程同时执行
#define muse_bind_async(...) \
    muse::rpc::Singleton<Registry>()->Bind(__VA_ARGS__);

//同步方法，一次只能一个线程执行此方法
#define muse_bind_sync(...) \
    muse::rpc::Singleton<SynchronousRegistry>()->Bind(__VA_ARGS__);

namespace muse::rpc{


    template <class T>
    std::shared_ptr<T> Singleton() {
        static std::shared_ptr<T> instance = std::make_shared<T>();
        return instance;
    }

    class Disposition{
    public:
        static std::string Prefix;

        static void Server_Configure();
          /*
           * @minThreadCount 线程池最小线程数
           * @maxThreadCount 线程池中最大线程数
           * @taskQueueLength 任务队列极限长度
           * @dynamicThreadVacantMillisecond 空闲线程数量
           * @logfile_directory 日志目录
           * */
        static void Server_Configure(
                  const size_t& minThreadCount,
                  const size_t& maxThreadCount,
                  const size_t& taskQueueLength,
                  const std::chrono::milliseconds& dynamicThreadVacantMillisecond,
                  const std::string& logfile_directory,
                  bool console_open_state  = true
        );

        static void Client_Configure();
    };


}

#endif //MUSE_SERVER_RPC_HPP
