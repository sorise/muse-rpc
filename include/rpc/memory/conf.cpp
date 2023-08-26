//
// Created by remix on 23-7-29.
//
#include "conf.hpp"
namespace muse::rpc{
    /* 内存池配置 */
    std::pmr::synchronized_pool_resource* make_memory_pool(){
        std::pmr::pool_options option;
        option.largest_required_pool_block = 1024*1024*5; //5M
        option.max_blocks_per_chunk = 4096; //每一个chunk有多少个block
        auto *pool = new std::pmr::synchronized_pool_resource(option);
        return pool;
    }

    std::shared_ptr<std::pmr::synchronized_pool_resource> MemoryPoolSingleton() {
        static std::shared_ptr<std::pmr::synchronized_pool_resource> instance =
                std::shared_ptr<std::pmr::synchronized_pool_resource>(make_memory_pool());
        return instance;
    }//问题在于没办法 回收 内存，调用析构函数。v
}