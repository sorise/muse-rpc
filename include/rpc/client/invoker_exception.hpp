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
    enum class ClientError: short {
        IPAddressError,
        CreateSocketError,                   //创建
        SocketFDError,                       //socket 是错误的
        ServerNotSupportTheProtocol,         //服务器地址错误，请求地址不支持当前协议
        PieceStateIncorrect,                 //状态设置错误
    };

    /* 错误异常消息 */
    class ClientException: public std::logic_error{
    public:
        explicit ClientException(const std::string &arg, ClientError number);
        ~ClientException() override = default;
        ClientError getErrorNumber(); //返回错误号
    private:
        ClientError errorNumber;
    };
}

#endif //MUSE_RPC_INVOKER_EXCEPTION_HPP
