//
// Created by remix on 23-7-22.
//
#include "response.hpp"

namespace muse::rpc{


    Response::Response(uint64_t _id, uint16_t _port, uint32_t _ip):
    Servlet(_id, _port, _ip),
    data(nullptr),
    is_data_prepared(false),
    ack_accept(0),
    piece_count(0),
    last_piece_size(0),
    total_data_size(0),
    pieceState(0)
    {

    }


}