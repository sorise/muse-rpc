//
// Created by remix on 23-8-20.
//

#ifndef MUSE_RPC_REACTOR_TRANSMITTER_HPP
#define MUSE_RPC_REACTOR_TRANSMITTER_HPP
#include "../client/transmitter.hpp"
#include "reactor.hpp"

// 1.0s
#define Reactor_Transmitter_Request_TimeOut 1000
// 0.9s
#define Reactor_Transmitter_Response_TimeOut 900

#define Reactor_Transmitter_Try_Times  3

#define Reactor_Transmitter_Read_GAP  300

/* 能够主动发起请求、支持NAT */
namespace muse::rpc{
    class ReactorTransmitter {
    public:
        /* 构造函数
         @_port 绑定的端口号
         @_worker_count 工作线程数量
         @_queue_size 线程池任务队列数
         */
        explicit ReactorTransmitter(uint16_t _port, uint32_t _sub_reactor_count, uint32_t _open_max_connection, ReactorRuntimeThread _type);

        bool send(TransmitterEvent &&event);

        using Message_Queue = std::map<Servlet , std::shared_ptr<TransmitterTask>>;

        /* 设置服务器请求阶段超时时间 */
        void set_request_timeout(const uint32_t& _timeout);

        void set_response_timeout(const uint32_t& _timeout);

        void set_try_time(const uint16_t& _try);

        void start();

        void stop() noexcept;

        virtual ~ReactorTransmitter();
    private:
        void loop();
        /* 将数据发生给服务器 */
        void send_data(const std::shared_ptr<TransmitterTask>& task);

        void timeout_event(Servlet servlet, uint64_t cur_ack);

        void request_ACK(const std::shared_ptr<TransmitterTask>& task);

        void send_response_ACK(const std::shared_ptr<TransmitterTask>& task, const uint16_t &_ack_number);

        void delete_message(Servlet servlet);

        void response_timeout_event(Servlet servlet, std::chrono::milliseconds last_active);

        /* 触发回调事件 */
        void trigger(Servlet servlet);

        void send_request_heart_beat(const std::shared_ptr<TransmitterTask>& task, CommunicationPhase phase);

        void send_heart_beat(const std::shared_ptr<TransmitterTask>& task, CommunicationPhase phase);

        void try_trigger(const std::shared_ptr<TransmitterTask>& msg);

        /* 启动从反应堆 */
        void startSubReactor();
    private:
        std::chrono::milliseconds recv_gap {Reactor_Transmitter_Read_GAP};
        /* 控制超时时间 毫秒值 */
        std::chrono::milliseconds request_timeout {Reactor_Transmitter_Request_TimeOut};
        std::chrono::milliseconds response_timeout {Reactor_Transmitter_Response_TimeOut};
        uint16_t try_time {Reactor_Transmitter_Try_Times};

        int socket_fd; //本地socket 需要进行循环
        uint16_t port; //端口
        std::shared_ptr<ThreadPool> workers;
        char buffer[Protocol::FullPieceSize + 1] {0};       // 发送缓存区
        char read_buffer[Protocol::FullPieceSize + 1]{0};  // 接受缓存区
        std::shared_ptr<std::pmr::synchronized_pool_resource> pool;
        //发送任务等待区
        TimerTree timer; //定时器
        std::mutex timer_mtx;
        std::mutex new_messages_mtx;
        std::condition_variable condition; //条件变量 用于阻塞或者唤醒线程
        Message_Queue messages;
        Message_Queue new_messages;         //新发送任务
        Protocol protocol;
        std::shared_ptr<std::thread> runner;    // 运行线程
        std::atomic<bool> run_state {false };

        ReactorRuntimeThread type;                                      // 运行方式
        uint32_t subReactorCount;                                       // 从反应堆的数量
        uint32_t counter;                                               // 计数器
        uint32_t openMaxConnection;                                     // 每个从反应堆维持的虚拟链接数量
        std::vector<std::unique_ptr<SubReactor>> subs;                  // 从反应堆s
    };
}


#endif //MUSE_RPC_REACTOR_TRANSMITTER_HPP
