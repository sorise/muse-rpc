//
// Created by remix on 23-8-13.
//
#include "outcome.hpp"
namespace muse::rpc{
    bool Outcome<void>::isOK() const{
        return response.getOkState() && (protocolReason == FailureReason::OK);
    }
}