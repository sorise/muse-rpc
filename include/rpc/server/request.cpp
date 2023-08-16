#include "reqeust.hpp"

namespace muse::rpc{

    /**
     * @_id 时间戳
     * @_port 端口号 小端序
     * @_ip IP地址 小端序
     */
    Servlet::Servlet(uint64_t _id, uint16_t _port, uint32_t _ip):
    request_id(_id),
    client_port(_port),
    client_ip_address(_ip){

    }

    bool operator <(const Servlet &me, const Servlet &other){
        if (me.request_id != other.request_id){
            return me.request_id < other.request_id;
        }else if(me.client_ip_address != other.client_ip_address){
            return me.client_ip_address > other.client_ip_address;
        }else {
            return me.client_port > other.client_port;
        }
    }

    uint64_t Servlet::getID() const {
        return request_id;
    }

    uint16_t Servlet::getHostPort() const {
        return client_port;
    }

    uint32_t Servlet::getIpAddress() const{
        return client_ip_address;
    }

    Request::Request(uint64_t _id, uint16_t _port, uint32_t _ip, uint16_t _pieces, uint32_t _data_size):
    Servlet(_id, _port, _ip),
    piece_count(_pieces),
    total_data_size(_data_size),
    piece_state(_pieces, false){

    }

    uint32_t Request::getTotalDataSize() const {
        return this->total_data_size;
    }

    uint16_t Request::getPieceCount() const {
        return piece_count;
    }

    bool Request::getPieceState(const uint16_t & _index) const {
        if (_index > piece_count) {
            SPDLOG_ERROR("Class Request getPieceState index Cross boundary error!, @_index {} vector size {}", _index, piece_state.size());
            throw ReactorException("[Request]", ReactorError::PieceStateOutBound);
        }else{
            return piece_state[_index];
        }
    }

    void Request::setSocket(int _socket_fd) {
        socketFd = _socket_fd;
    }

    int Request::getSocket() const {
        return socketFd;
    }

    uint16_t Request::setPieceState(const uint16_t & _index, bool value) {
        if (_index > piece_count) {
            SPDLOG_ERROR("Class Request getPieceState index Cross boundary error!, @_index {} vector size {}", _index, piece_state.size());
            throw ReactorException("[Request]", ReactorError::PieceStateOutBound);
        }else{
            piece_state[_index] = value;
            return getAckNumber();
        }
    }

    uint16_t Request::getAckNumber() {
        uint16_t result = piece_state.size();
        for (int i = 0; i < piece_state.size(); ++i) {
            if (!piece_state[i]){
                result = i;
                break;
            }
        }
        return result;
    }

    bool Request::getTriggerState() const {
        return is_trigger;
    }

    void Request::trigger() {
        is_trigger = true;
    }
}


