//
// Created by remix on 23-8-16.
//

#include "peer.hpp"

namespace muse::rpc{

    bool operator <(const Peer &me, const Peer &other){
        if(me.client_ip_address != other.client_ip_address){
            return me.client_ip_address > other.client_ip_address;
        }else {
            return me.client_port > other.client_port;
        }
    }

    Peer::Peer(uint16_t _port, uint32_t _ip):
    client_port(_port),
    client_ip_address(_ip)
    {

    }

    uint16_t Peer::getHostPort() const {
        return this->client_port;
    }

    uint32_t Peer::getIpAddress() const {
        return this->client_ip_address;
    }

    bool Peer::operator==(const Peer &oth) const {
        if (oth.client_port == this->client_port && oth.client_ip_address == this->client_ip_address){
            return true;
        }
        return false;
    }
}
