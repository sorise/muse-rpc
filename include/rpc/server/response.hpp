// Created by remix on 23-7-22.

#ifndef MUSE_RPC_RESPONSE_HPP
#define MUSE_RPC_RESPONSE_HPP
#include "reqeust.hpp"
#include "../memory/conf.hpp"
#include "../protocol/protocol.hpp"

//在客户端 由客户端负载创建
namespace muse::rpc{
    class Response: public Servlet{
    public:
        static constexpr int tryTimes = 3;   //尝试次数
        static constexpr long timeout = 120; //ms
    private:
        std::vector<bool>               piece_state;                  // 每个分片的状态
        uint16_t                        piece_count;                  // 有多少个分片
        uint16_t                        last_piece_size;              // 最后一个分片的大小
        uint32_t                        total_data_size;              // 总共有多少数据
        uint16_t                        ack_accept;                   // 已经收到的最大 ack
        bool                            is_get_new_ack {true};        //是否得到新的客户端响应
    public:
        std::shared_ptr<std::pmr::synchronized_pool_resource> pool {nullptr};
        std::chrono::milliseconds       lastActive;         //标记什么时候收到了客户端发来的消息，初始化使用响应数据已经准备好的时间
        char*  data;               //数据
        Response();
        Response(uint64_t _id, uint16_t _port, uint32_t _ip, uint16_t _pieces, uint32_t _data_size, char* _data);
        Response(const Response &other);
        ~Response() override;
        uint32_t getTotalDataSize() const;
        uint16_t getPieceCount() const;
        bool getPieceState(const uint32_t & _idx) const;
        void setPieceState(const uint32_t & _index, bool value);
        uint16_t getAckAccept() const;
        uint16_t getLastPieceSize() const;
        bool setAckAccept(uint16_t _new_ack);
        bool getNewAckState() const;
        void setNewAckState(bool value);
    };
}
#endif //MUSE_RPC_RESPONSE_HPP