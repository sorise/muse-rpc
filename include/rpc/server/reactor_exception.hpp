//
// Created by remix on 23-7-20.
//

#ifndef MUSE_REACTOR_EXCEPTION_HPP
#define MUSE_REACTOR_EXCEPTION_HPP
#include <iostream>
#include <cstring>
#include <vector>
#include <exception>
#include <tuple>

namespace muse::rpc{



    /* 错误号 */
    enum class ReactorError: short {
        EpollFdCreateFailed,
        EpollEPOLL_CTL_ADDFailed,
        SocketFdCreateFailed,
        SocketBindFailed,
        CreateSubReactorFailed,
        CreateMainReactorThreadFailed,
        CreateTransmitterThreadFailed,
        MemoryPoolCreateFailed,
        PieceStateOutBound,  // 访问越界错误
    };

    /* 错误异常消息 */
    class ReactorException: public std::logic_error{
    public:
        explicit ReactorException(const std::string &arg, ReactorError number);
        ~ReactorException() override = default;
        ReactorError getErrorNumber(); //返回错误号
    private:
        ReactorError errorNumber;
    };


}
#endif //MUSE_REACTOR_EXCEPTION_HPP
