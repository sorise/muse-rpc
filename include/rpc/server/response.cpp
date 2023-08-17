//
// Created by remix on 23-7-22.
//
#include "response.hpp"

namespace muse::rpc{


    Response::Response():
    Servlet(0, 0, 0),
    data(nullptr),
    ack_accept(0),
    piece_count(0),
    total_data_size(0),
    piece_state(0),
    last_piece_size(0)
    {
        lastActive = GetNowTick();
    }

    Response::Response(uint64_t _id, uint16_t _port, uint32_t _ip, uint16_t _pieces, uint32_t _data_size, char*  _data):
            Servlet(_id, _port, _ip),
            data(_data),
            ack_accept(0),
            piece_count(_pieces),
            total_data_size(_data_size),
            piece_state(0)
    {
        last_piece_size = (_data_size % Protocol::defaultPieceLimitSize == 0)? Protocol::defaultPieceLimitSize:_data_size % Protocol::defaultPieceLimitSize;
        lastActive = GetNowTick();
    }

    Response::Response(const Response &other):
    Servlet(other),
    data(other.data),
    ack_accept(other.ack_accept),
    piece_count(other.piece_count),
    total_data_size(other.total_data_size),
    last_piece_size(other.last_piece_size),
    pool(other.pool),
    piece_state(other.piece_state){

    }

    uint32_t Response::getTotalDataSize() const {
        return total_data_size;
    }

    uint16_t Response::getPieceCount() const {
        return piece_count;
    }

    bool Response::getPieceState(const uint32_t &_index) const {
        if (_index > piece_count) {
            SPDLOG_ERROR("Class Request getPieceState index Cross boundary error!, @_index {} vector size {}", _index, piece_state.size());
            throw ReactorException("[Request]", ReactorError::PieceStateOutBound);
        }else{
            return piece_state[_index];
        }
    }

    void Response::setPieceState(const uint32_t &_index, bool value) {
        if (_index > piece_count) {
            SPDLOG_ERROR("Class Request getPieceState index Cross boundary error!, @_index {} vector size {}", _index, piece_state.size());
            throw ReactorException("[Request]", ReactorError::PieceStateOutBound);
        }else{
            piece_state[_index] = value;
        }
    }

    uint16_t Response::getAckAccept() const {
        return ack_accept;
    }

    bool Response::setAckAccept(uint16_t _new_ack) {
        if (_new_ack > ack_accept){
            ack_accept = _new_ack;
            return true;
        }
        return false;
    }

    uint16_t Response::getLastPieceSize() const {
        return last_piece_size;
    }

    void Response::setNewAckState(bool value) {
        is_get_new_ack = value;
    }

    bool Response::getNewAckState() const {
        return this->is_get_new_ack;
    }

    Response::~Response() {
        if (pool == nullptr){
            MemoryPoolSingleton()->deallocate(data, total_data_size);
        }else {
            pool->deallocate(data, total_data_size);
        };

    }


}