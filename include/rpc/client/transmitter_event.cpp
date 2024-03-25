//
// Created by remix on 23-8-13.
//

#include "transmitter_event.hpp"



namespace muse::rpc{
    TransmitterEvent::TransmitterEvent(std::string _ip_address, const uint16_t& _port)
    :ip_address(std::move(_ip_address)),
    port(_port),
    serializer(),
    is_set_callback(false),
    is_set_remote_func(false),
    remote_process_name(){

    }

    TransmitterEvent::TransmitterEvent(TransmitterEvent&& event) noexcept
    :ip_address(std::move(event.ip_address)),
    port(event.port),
    serializer(std::move(event.serializer)),
    remote_process_name(std::move(event.remote_process_name)),
    is_set_callback(event.is_set_callback),
    is_set_remote_func(event.is_set_remote_func),
    callBack(std::move(event.callBack))
    {

    }

    const std::string &TransmitterEvent::get_ip_address() {
        return this->ip_address;
    }

    const BinarySerializer& TransmitterEvent::get_serializer(){
        return this->serializer;
    }

    const uint16_t &TransmitterEvent::get_port() {
        return this->port;
    }

    void TransmitterEvent::trigger_callBack(ResponseData* responseData) {
        this->callBack(responseData);
    }

    bool TransmitterEvent::get_callBack_state() const {
        return this->is_set_callback;
    }

    bool TransmitterEvent::get_remote_state() const {
        return this->is_set_remote_func;
    }

    TransmitterEvent::~TransmitterEvent() {

    }
}