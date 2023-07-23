//
// Created by remix on 23-7-22.
//

#ifndef MUSE_RPC_RESPONSE_HPP
#define MUSE_RPC_RESPONSE_HPP
#include "reqeust.hpp"

//在客户端 由客户端负载创建
namespace muse::rpc{
    class Response: Servlet{
        std::vector<bool>               pieceState;         // 每个分片的状态
        uint16_t                        piece_count;        // 有多少个分片
        uint16_t                        last_piece_size;    // 最后一个分片的大小
        uint32_t                        total_data_size;    // 总共有多少数据
        int                             socket_fd;          // socket
        uint16_t                        ack_accept;         // 已经收到的最大ack
        bool                            is_data_prepared;
    public:
        std::shared_ptr<char[]>         data;               //已经接受了多少数据
        uint32_t                        has_send_distance;  //已经发送了多少数据
        Response(uint64_t _id, uint16_t _port, uint32_t _ip);
    };

}



#endif //MUSE_RPC_RESPONSE_HPP
