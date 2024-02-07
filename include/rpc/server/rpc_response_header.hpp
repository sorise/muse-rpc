#ifndef MUSE_RPC_RPC_RESPONSE_HEADER_HPP
#define MUSE_RPC_RPC_RESPONSE_HEADER_HPP
#include <iostream>
#include "serializer/IbinarySerializable.h"

namespace muse::rpc{
    enum class RpcFailureReason:int{
        Success = 0, // 成功
        ParameterError = 1, // 参数错误,
        MethodNotExist = 2, // 指定方法不存在
        ClientInnerException = 3, // 客户端内部异常，请求还没有到服务器
        ServerInnerException = 4, // 服务器内部异常，请求到服务器了，但是处理过程有异常
        MethodExecutionError = 5, // 方法执行错误
        UnexpectedReturnValue = 6, //返回值非预期
    };

    class RpcResponseHeader: public muse::serializer::IBinarySerializable{
    public:
        static std::string prefix_name;
    private:
        bool ok;
        int reason;
    public:
        MUSE_IBinarySerializable(ok, reason);
        RpcResponseHeader();
        RpcResponseHeader(const RpcResponseHeader &other);
        void setOkState(const bool& _ok);
        bool getOkState() const;
        int getReason() const;
        void setReason(const RpcFailureReason& _reason);
    };
}

#endif //MUSE_RPC_RPC_RESPONSE_HEADER_HPP
