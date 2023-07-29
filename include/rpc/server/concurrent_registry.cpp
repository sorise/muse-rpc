//
// Created by remix on 23-7-29.
//

#include "concurrent_registry.hpp"

namespace muse::rpc{

    void ConcurrentRegistry::runSafely(const std::string &name, BinarySerializer *serializer) {
        std::lock_guard<std::mutex> lock(concurrent_dictionary[name]->mtx);
        concurrent_dictionary[name]->func((serializer));
    }

    void ConcurrentRegistry::runEnsured(const std::string &name, BinarySerializer *serializer) {
        std::lock_guard<std::mutex> lock(concurrent_dictionary[name]->mtx);
        concurrent_dictionary[name]->func((serializer));
    }

    bool ConcurrentRegistry::check(const std::string &name) {
        return concurrent_dictionary.find(name) == concurrent_dictionary.end();
    }

    Controller::Controller(std::function<void(BinarySerializer *)>&& f)
    :func(std::move(f)), mtx(){

    }
}
