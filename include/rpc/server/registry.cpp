//
// Created by remix on 23-7-26.
//

#include "registry.hpp"

namespace muse::rpc{

    bool Registry::check(const std::string &name) {
        return dictionary.find(name) != dictionary.end();
    }

    void Registry::runSafely(const std::string &name, BinarySerializer *serializer) {
        if (check(name)) dictionary[name](serializer);
    }

    void Registry::runEnsured(const std::string &name, BinarySerializer *serializer) {
        dictionary[name](serializer);
    }

}
