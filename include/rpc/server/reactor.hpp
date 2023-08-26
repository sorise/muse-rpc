//
// Created by remix on 23-7-17.
//

#ifndef MUSE_SERVER_REACTOR_HPP
#define MUSE_SERVER_REACTOR_HPP
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory>
#include <memory_resource>
#include <thread>
#include <atomic>
#include <map>
#include <iostream>
#include <arpa/inet.h>
#include "../protocol/protocol.hpp"
#include "../logger/conf.hpp"
#include "reqeust.hpp"
#include "reactor_exception.hpp"
#include "sub_reactor.hpp"
#include "transmitter_link_reactor.hpp"
#include "../memory/conf.hpp"
#include "global_entry.hpp"
#include "spdlog/spdlog.h"
#include "../client/transmitter.hpp"
#include "../client/global_id.hpp"

/* 主要负责建立链接 */
namespace muse::rpc{

    enum class ReactorRuntimeThread: int {
        Synchronous = 1,  // 同步方式，反应堆在当前线程运行
        Asynchronous = 2  // 反应堆新开一个线程运行
    };

    //返回一个内存池
    std::shared_ptr<std::pmr::synchronized_pool_resource> getMemoryPool(size_t _largest_required_pool_block, size_t _max_blocks_per_chunk);

    class Reactor{
    private:
        uint16_t port;                                                  // 监听端口
        int epollFd;                                                    // EPOLL ID
        int socketFd;                                                   // Socket ID
        std::atomic<bool> runState {false};                           // 运行状态
        std::shared_ptr<std::thread> runner;                            // 运行线程
        ReactorRuntimeThread type;                                      // 运行方式
        std::shared_ptr<std::pmr::synchronized_pool_resource> pool;     // 内存池
        uint32_t subReactorCount;                                       // 从反应堆的数量
        uint32_t counter;                                               // 计数器
        uint32_t openMaxConnection;                                     // 每个从反应堆维持的虚拟链接数量
        std::vector<std::unique_ptr<SubReactor>> subs;                  // 从反应堆
        Protocol protocol;                                              // 协议对象
        std::unique_ptr<TransmitterLinkReactor> transmitter_linker  { nullptr };

        bool is_start_transmitter {false}; /*  */
    private:
        /* 启动从反应堆 */
        void startSubReactor();
        /* 开启 EPOLL 循环， start 方法启动的就是这个函数 */
        void loop();

        void start_transmitter();
    public:
        Reactor(uint16_t _port, uint32_t _sub_reactor_count, uint32_t _open_max_connection, ReactorRuntimeThread _type);
        Reactor(const Reactor &other) = delete; //拷贝也不行
        Reactor(Reactor &&other) = delete;  //移动也不允许
        Reactor& operator =(const Reactor &other) = delete;
        Reactor& operator =(Reactor &&other) = delete;

        /* 有可能被其他线程执行 */
        bool send(TransmitterEvent &&event);

        /* 启动发射器，启动之前需要首先开启 start 方法 */

        /* 启动反应堆 */
        void start(bool _is_start_transmitter = false);

        // 首先关闭主反应堆，再关闭从反应堆
        void stop() noexcept;
        //等待发送器启动完毕
        void wait_transmitter();

        virtual ~Reactor();
    };

}
#endif //MUSE_SERVER_REACTOR_HPP
