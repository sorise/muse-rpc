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
#include "../../logger/conf.hpp"
#include "reqeust.hpp"
#include "virtual_connection.hpp"

namespace muse::rpc{

    //多长时间不发消息，就删除掉这个 socket 2 分钟
    class SubReactor{
    public:
        static constexpr const std::chrono::milliseconds ConnectionTimeOut = 120000ms;
    private:
        uint16_t port;                                                        //当前端口
        Protocol protocol_util{};
        int openMaxConnection;                                                //最多维持的链接数量
        int epollFd;
        std::mutex newConLocker;
        std::queue<std::tuple<int, size_t, sockaddr_in, std::shared_ptr<char[]>>> newConnections;
        std::map<Servlet, Request, std::less<>> messages;     //存储报文信息
        std::mutex conLocker;
        int lastEmptyPlace {0};
        VirtualConnection* connections;                                       //存储连接信息 runner 线程会使用  reactor 线程也会使用
        int activeConnectionsCount {0};                                       //目前维持的链接数量
        std::shared_ptr<std::pmr::synchronized_pool_resource> pool;           //内存池
        std::shared_ptr<std::thread> runner;                                  //运行线程
        std::atomic<bool> runState;                                           //是否在运行
        /*
         * 处理新的链接
         * 如果当前系统维护的链接数量小于(epoll中的socket数量小于) openMaxConnection，则加入到 epoll中
         * 并且根据报文类型进行响应处理
         * 否则调用 sendExhausted 发送服务器资源耗尽报文,然后关闭 socket。
         * */
        void dealWithNewConnection();

        void sendACK(int _socket_fd, uint16_t _ack_number, uint64_t _message_id);
        void sendReset(int _socket_fd, uint64_t _message_id);

        /*
         * 查询是否还有存放socket的空缺位置，如果有返回查找成功，并且返回数组索引
         * 否则返回失败
         * */
        std::pair<bool, size_t> findConnectionsEmptyPlace();

        int clearTimeoutConnections();
        void sendHeartbeat(int _socket_fd, uint64_t _message_id);
        void sendExhausted(int _socket_fd);
    public:
        //有一次内存复制, 丢到 connections 和 Epoll loop 中
        void acceptConnection(int _socketFd, size_t _recv_length, sockaddr_in addr ,const std::shared_ptr<char[]>& data);

        SubReactor(uint16_t _port, int _open_max_connection,std::shared_ptr<std::pmr::synchronized_pool_resource> _pool);
        SubReactor(const SubReactor &other) = delete;
        SubReactor& operator=(const SubReactor &other) = delete;
        SubReactor(SubReactor &&other) = delete;
        SubReactor& operator=(SubReactor &&other) = delete;

        void loop();
        void start();

        ~SubReactor();
    };


}

#endif //MUSE_SERVER_SUB_REACTOR_HPP
