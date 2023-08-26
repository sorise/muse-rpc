//
// Created by remix on 23-8-22.
//

#ifndef MUSE_RPC_TRANSMITTER_LINK_REACTOR_HPP
#define MUSE_RPC_TRANSMITTER_LINK_REACTOR_HPP
#include "../client/transmitter.hpp"
#include "global_entry.hpp"

namespace muse::rpc{

    class TransmitterLinkReactor: public Transmitter{
    public:
        using New_Data_Tuple_Type = std::tuple<uint64_t, std::shared_ptr<char[]>, size_t, sockaddr_in>;
    private:
        void loop() override;
        void try_trigger(const std::shared_ptr<TransmitterTask>& msg) override;
    private:
        std::mutex queue_mtx;
        std::mutex wait_new_data_mtx;
        std::queue<New_Data_Tuple_Type> newData;                     // 新到的链接
        std::condition_variable waiting_newData_condition;
        std::atomic<bool> is_start{false} ;
    public:
        void acceptNewData(uint64_t id, std::shared_ptr<char[]> data, size_t size, sockaddr_in addr);
        TransmitterLinkReactor(int _socket_fd, const std::shared_ptr<ThreadPool> &_workers);
        bool send(TransmitterEvent &&event, uint64_t msg_id);
        ~TransmitterLinkReactor() override;
        //是否已经启动完成
        bool start_finish();
    };


    //新消息存储

}


#endif //MUSE_RPC_TRANSMITTER_LINK_REACTOR_HPP
