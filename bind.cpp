//
// Created by remix on 23-6-23.
// linux 不是有定时器信号
#include <iostream>
#include <chrono>
#include <limits>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <functional>
#include <cstring>
#include <future>
#include <memory_resource>

std::pmr::synchronized_pool_resource* make_memory_pool(){
    std::pmr::pool_options option;
    option.largest_required_pool_block = 1024*1024*5; //5M
    option.max_blocks_per_chunk = 2048; //每一个chunk有多少个block
    auto *pool = new std::pmr::synchronized_pool_resource(option);
    return pool;
}

std::shared_ptr<char[]> btest(std::shared_ptr<char[]> pt){
    std::shared_ptr<char[]> dt = pt;
    return dt;
}

int main()
{
    std::shared_ptr<std::pmr::synchronized_pool_resource> pool(make_memory_pool()) ;

    for (int i = 0; i < 1000; ++i) {
        char* d= (char*)pool->allocate(14);
        std::shared_ptr<char[]> dt(d, [=](char *ptr){
            printf("delete\n");
            pool->deallocate(d,1564);
        } );
        auto at = btest(dt);
        printf("---- %d\n", i);
    }

    return 0;
}
