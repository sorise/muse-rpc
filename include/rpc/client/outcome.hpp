//
// Created by remix on 23-7-27.
//

#ifndef MUSE_RPC_OUTCOME_HPP
#define MUSE_RPC_OUTCOME_HPP
#include "../server/rpc_response_header.hpp"
#include "response_data.hpp"

namespace muse::rpc{
    template<typename R>
    struct Outcome {
        R value;
        RpcResponseHeader response;
        FailureReason protocolReason {FailureReason::OK};
        bool isOK() const;
    };

    template<>
    struct Outcome<void> {
        RpcResponseHeader response;
        FailureReason protocolReason {FailureReason::OK};
        bool isOK() const;
    };

    template<typename R>
    bool Outcome<R>::isOK() const{
        return response.getOkState() && (protocolReason == FailureReason::OK);
    }
}

#endif //MUSE_RPC_OUTCOME_HPP
