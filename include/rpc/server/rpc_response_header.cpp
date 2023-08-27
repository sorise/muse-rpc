#include "rpc_response_header.hpp"

namespace muse::rpc{
    std::string RpcResponseHeader::prefix_name = {"@context/"};

    void RpcResponseHeader::setOkState(const bool& _ok){
        this->ok = _ok;
    }

    bool RpcResponseHeader::getOkState() const{
        return this->ok;
    }

    int RpcResponseHeader::getReason() const{
        return this->reason;
    }

    void RpcResponseHeader::setReason(const RpcFailureReason& _reason){
        this->reason = (int)_reason;
    }

    RpcResponseHeader::RpcResponseHeader()
    :ok(false),reason((int)RpcFailureReason::Success) {

    }

    RpcResponseHeader::RpcResponseHeader(const RpcResponseHeader &other)
    :ok(other.ok),reason(other.reason){

    }
}