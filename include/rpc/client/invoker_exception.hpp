//
// Created by remix on 23-7-23.
//

#ifndef MUSE_RPC_INVOKER_EXCEPTION_HPP
#define MUSE_RPC_INVOKER_EXCEPTION_HPP
#include <exception>
#include <string>
#include<iostream>

namespace muse::rpc{
    /* 错误号 */
    enum class InvokerError: short {
        IPAddressError,
        CreateSocketError,                   //创建
        SocketFDError,                       //socket 是错误的
        ServerNotSupportTheProtocol,         //服务器地址错误，请求地址不支持当前协议
    };

    /* 错误异常消息 */
    class InvokerException: public std::logic_error{
    public:
        explicit InvokerException(const std::string &arg, InvokerError number);
        ~InvokerException() override = default;
        InvokerError getErrorNumber(); //返回错误号
    private:
        InvokerError errorNumber;
    };
}

#endif //MUSE_RPC_INVOKER_EXCEPTION_HPP
