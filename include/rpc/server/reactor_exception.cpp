//
// Created by remix on 23-7-20.
//
#include "reactor_exception.hpp"

namespace muse::rpc{
    ReactorException::ReactorException(const std::string &arg, ReactorError number)
            :logic_error(arg), errorNumber(number) {}

    ReactorError ReactorException::getErrorNumber() {
        return errorNumber;
    }
}
