#ifndef MUSE_RPC_TRANSMITTER_HPP
#define MUSE_RPC_TRANSMITTER_HPP
#include <arpa/inet.h>
#include <unordered_map>
#include <map>

#include "transmitter_event.hpp"
#include "thread_pool/pool.hpp"
#include "../server/middleware_channel.hpp"
#include "global_id.hpp"
#include "timer/timer_tree.hpp"
#include "response_data.hpp"

using namespace muse::pool;
using namespace muse::timer;

namespace muse::rpc {
    struct TransmitterTask{
        ResponseData response_data;  /* 存放响应数据 */
        TransmitterEvent event; /* 等待触发的事件 */

        /* 获得最终发送数据 */
        CommunicationPhase phase;               // 当前所处的阶段
        uint32_t total_size { 0 };              // 总数据量
        uint16_t piece_count{ 0 };              // 分片数量
        uint64_t message_id {};                 // 消息 ID
        std::shared_ptr<char[]> data;           // 最后发送的数据位置
        uint16_t ack_accept = 0;                // ack
        uint16_t ack_expect = 0;                // ack

        //发送服务状态控制
        int timeout_time {0};        //发送超时的次数

        uint16_t send_time = {0};
        uint16_t last_piece_size { 0 };  //最后一个分片大小
        std::chrono::milliseconds last_active {0};  //活跃时间

        struct sockaddr_in server_address{};

        bool is_trigger {false};

        TransmitterTask(TransmitterEvent && _event, const uint64_t& message_id);
    };

    enum class TransmitterThreadType: int {
        Synchronous = 1,  // 同步方式，发送器当前线程运行
        Asynchronous = 2  // 发送器新开一个线程运行
    };

    // 1.0s
    #define Transmitter_Request_TimeOut 1000
    // 0.9s
    #define Transmitter_Response_TimeOut 900

    #define Transmitter_Try_Times  3

    #define Transmitter_Read_GAP  500

    class Transmitter {
    public:
        /* 构造函数
         @_port 绑定的端口号
         @_worker_count 工作线程数量
         @_queue_size 线程池任务队列数
         */
        explicit Transmitter(const uint16_t& _port, size_t coreThreadsCount = 2, size_t _maxThreadCount = 3, size_t _queueMaxSize = 4096, ThreadPoolType _type = ThreadPoolType::Flexible,  ThreadPoolCloseStrategy poolCloseStrategy = muse::pool::ThreadPoolCloseStrategy::WaitAllTaskFinish, std::chrono::milliseconds unit = 1500ms);

        Transmitter(bool flag ,int _socket_fd, const std::shared_ptr<ThreadPool> &_workers);

        bool send(TransmitterEvent &&event);

        using Message_Queue = std::unordered_map<uint64_t , std::shared_ptr<TransmitterTask>>;

        void set_request_timeout(const uint32_t& _timeout);

        void set_response_timeout(const uint32_t& _timeout);

        void set_try_time(const uint16_t& _try);

        void start(TransmitterThreadType type);

        void stop() noexcept;

        void stop_immediately() noexcept;

        virtual ~Transmitter();
    protected:
        /* 用于发送和接受数据 */
        virtual void loop();
        /* 将数据发生给服务器 */
        void send_data(const std::shared_ptr<TransmitterTask>& task);

        void timeout_event(uint64_t message_id, uint64_t cur_ack);

        void request_ACK(const std::shared_ptr<TransmitterTask>& task);

        void send_response_ACK(const std::shared_ptr<TransmitterTask>& task, const uint16_t &_ack_number);

        void delete_message(uint64_t message_id);

        void response_timeout_event(uint64_t message_id, std::chrono::milliseconds last_active);

        /* 触发回调事件 */
        void trigger(std::shared_ptr<TransmitterTask> msg);

        void send_request_heart_beat(const std::shared_ptr<TransmitterTask>& task, CommunicationPhase phase);

        void send_heart_beat(const std::shared_ptr<TransmitterTask>& task, CommunicationPhase phase);

        virtual void try_trigger(const std::shared_ptr<TransmitterTask>& msg);

    protected:
        std::chrono::milliseconds recv_gap {Transmitter_Read_GAP};
        /* 控制超时时间 毫秒值 */
        std::chrono::milliseconds request_timeout {Transmitter_Request_TimeOut};
        std::chrono::milliseconds response_timeout {Transmitter_Response_TimeOut};
        uint16_t try_time {Transmitter_Try_Times};

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

        std::mutex condition_mtx;
        std::condition_variable condition; //条件变量 用于阻塞或者唤醒线程
        Message_Queue messages;
        Message_Queue new_messages;         //新发送任务
        Protocol protocol;
        std::shared_ptr<std::thread> runner;    // 运行线程
        std::atomic<bool> run_state {false };
    };
} // muse::rpc

#endif //MUSE_RPC_TRANSMITTER_HPP
