//
// Created by remix on 23-7-22.
//
#include "response.hpp"

namespace muse::rpc{

    Response::Response(uint64_t _id, uint16_t _port, uint32_t _ip, uint16_t _pieces, uint32_t _data_size, std::shared_ptr<char[]>  _data):
    Servlet(_id, _port, _ip),
    data(_data),
    is_data_prepared(false),
    ack_accept(0),
    piece_count(_pieces),
    total_data_size(_data_size),
    piece_state(0)
    {
        last_piece_size = (_data_size % Protocol::defaultPieceLimitSize == 0)? Protocol::defaultPieceLimitSize:_data_size % Protocol::defaultPieceLimitSize;
        lastActive = GetNowTick();
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

}