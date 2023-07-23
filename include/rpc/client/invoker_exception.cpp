//
// Created by remix on 23-7-23.
//

#include "invoker_exception.hpp"

namespace muse::rpc{
    /* 异常构造函数 */
    InvokerException::InvokerException(const std::string &arg, InvokerError number)
            :logic_error(arg), errorNumber(number) {
    }

    /* 获取错误号 */
    InvokerError InvokerException::getErrorNumber() {
        return errorNumber;
    }
}