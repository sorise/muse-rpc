#ifndef MUSE_SERVER_SUB_REACTOR_HPP
#define MUSE_SERVER_SUB_REACTOR_HPP
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory>
#include <mutex>
#include <queue>
#include <memory_resource>
#include <thread>
#include <atomic>
#include <map>
#include <set>
#include <iostream>
#include <arpa/inet.h>
#include "../protocol/protocol.hpp"
#include "../logger/conf.hpp"
#include "reqeust.hpp"
#include "virtual_connection.hpp"
#include "reactor_pool.hpp"
#include "serializer/binarySerializer.h"
#include "timer/timer_tree.hpp"
#include "middleware_channel.hpp"
#include "global_entry.hpp"
#include "serializer/binaryDeserializeView.hpp"
#include "rpc_response_header.hpp"

namespace muse::rpc{
    //多长时间不发消息，就删除掉这个 socket 2 分钟
    class SubReactor{
    public:
        using message_queue = std::map<Servlet, Request, std::less<>>;
        using connection_tuple_type = std::tuple<int, size_t, sockaddr_in, std::shared_ptr<char[]>, bool>;
    public:
        //客户端 connect 超时时间单位 2 分钟
        static constexpr const std::chrono::milliseconds ConnectionTimeOut = 120000ms; //120000
        //request 超时时间
        static constexpr const std::chrono::milliseconds RequestTimeOut = 10000ms;
        //response 超时时间
        static constexpr const std::chrono::milliseconds ResponseTimeOut = 15000ms;
    private:
        uint16_t port;                                                        // 当前端口
        Protocol protocol_util{};                                             // 协议对象
        int openMaxConnection;                                                // 最多维持的链接数量
        int epollFd;                                                          // epoll ID
        std::mutex newConLocker;                                              // 操作 newConnections 的互斥量
        std::queue<connection_tuple_type> newConnections;                     // 新到的链接
        message_queue messages;                                               // 存储报文信息
        VirtualConnection* connections;                                       // 存储连接信息 runner 线程会使用  reactor 线程也会使用
        int lastEmptyPlace {0};                                               // connections的 空缺位置
        int activeConnectionsCount {0};                                       // 目前维持的链接数量
        std::shared_ptr<std::pmr::synchronized_pool_resource> pool;           // 内存池
        std::shared_ptr<std::thread> runner;                                  // 运行线程
        std::atomic<bool> runState {false };                                // 是否在运行
        std::mutex treeTimer_mtx;                                             // 也需要互斥量
        muse::timer::TimerTree treeTimer;                                     // 定时器
        //发送缓存区
        char sendBuf[Protocol::FullPieceSize + 1] = { '\0' };

        int epoll_switch_fd = -1;

        muse::serializer::ByteSequence sq  { muse::serializer::getByteSequence()};
    private:
        /*
         * 处理新的链接
         * 如果当前系统维护的链接数量小于(epoll中的socket数量小于) openMaxConnection，则加入到 epoll中
         * 并且根据报文类型进行响应处理
         * 否则调用 sendExhausted 发送服务器资源耗尽报文,然后关闭 socket。
         */
        void dealWithNewConnection();
        /*
         * 发送一阶段的ACK ProtocolType::ReceiverACK
         */
        void sendACK(int _socket_fd, uint16_t _ack_number, uint64_t _message_id);

        void sendACK(int _socket_fd, struct sockaddr_in addr,uint16_t _ack_number,  uint64_t _message_id);
        /*
         * 发送链接已重置报文
         */
        void sendReset(int _socket_fd, uint64_t _message_id, CommunicationPhase phase);
        /*
         * 查询是否还有存放socket的空缺位置，如果有返回查找成功，并且返回数组索引
         * 否则返回失败
         */
        std::pair<bool, size_t> findConnectionsEmptyPlace();
        /* 删除掉超时的虚拟连接 */
        int clearTimeoutConnections();
        /*
         * 发送心跳数据
         */
        void sendHeartbeat(int _socket_fd, uint64_t _message_id, CommunicationPhase phase = CommunicationPhase::Request);
        /* 服务器已经资源耗尽了 */
        void sendExhausted(int _socket_fd);
        /*
         * 开启epoll，一般开启一个新线程调用
         */
        void loop();
        /*
         * 发送请求 ACK，默认是一阶段
         * */
        void sendRequireACK(int _socket_fd, uint64_t _message_id, uint16_t _accept_min_order, CommunicationPhase phase = CommunicationPhase::Response);
        /*
         * 防止request一直在 message 队列中，对于超过 SubReactor::timeout 时间没有客户端响应的
         * message 删除掉！
         */
        void setResponseClearTimeout(VirtualConnection *vir, uint64_t _message_id);

        /* 判断响应对象是否应该删除 */
        void judgeResponseClearTimeout(VirtualConnection *vir, uint64_t _message_id);

        void setRequestTimeOutEvent(const Servlet& servlet);
        /** 判断是否可以删除 request */
        void judgeDelete(const Servlet& _servlet);
        /**
         * 已经收到所有的报文内容。需要上交给上级进行处理
         * Pair.first 数据获取接口
         * VirtualConnection* 结果获取
         * 全部由线程池来处理
         */
        void trigger(uint32_t data_size, std::shared_ptr<char[]> _full_data, Servlet servlet, VirtualConnection *vir);

        /**
         * 执行实际的任务，对完整报文进行解析！
         * 由线程池执行！
         * */
        void triggerTask(uint32_t data_size, std::shared_ptr<char[]> _full_data, Servlet servlet, VirtualConnection *vc);

        /** 关闭一个 connect udp socket,并且将其从 epoll 中 移除 */
        void closeSocket(VirtualConnection *vc) const;

        /** 需要改造，不应该拥有迭代器，而应该只拥有发送的数据 */
        void sendResponseDataToClient(VirtualConnection *vir, uint64_t _message_id);

        void checkDeleteSocket(VirtualConnection *vir);
    public:
        /** 主反应堆 将新链接传输到 从反应堆的接口
         * 有一次内存复制, 丢到 connections 和 Epoll loop 中 */
        bool acceptConnection(int _socketFd, size_t _recv_length, sockaddr_in addr ,const std::shared_ptr<char[]>& data, bool new_connection);
        /** 构造函数
         * 需要注入一个内存储 */
        SubReactor(uint16_t _port, int _open_max_connection,std::shared_ptr<std::pmr::synchronized_pool_resource> _pool);
        SubReactor(const SubReactor &other) = delete;
        SubReactor& operator=(const SubReactor &other) = delete;
        SubReactor(SubReactor &&other) = delete;
        SubReactor& operator=(SubReactor &&other) = delete;
        /* 启动方法，对外接口，开启一个线程运行 loop 方法 */
        void start();
        /* 停止线程运行，无法再次 start ! */
        void stop();
        /* 析构函数，需要关闭线程，还需要清理资源，包括epoll、request、新链接、现有的虚拟连接对象 */
        ~SubReactor();
    };
}
#endif //MUSE_SERVER_SUB_REACTOR_HPP
