#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "thread_pool/pool.hpp"
#include "thread_pool/concurrent_pool.hpp"

using namespace muse::pool;

ThreadPool<ThreadPoolType::Flexible, 1024, 8> pool(4, ThreadPoolCloseStrategy::WaitAllTaskFinish);

TEST_CASE("normal - commit", "[ThreadPool]"){
    auto ex = make_executor([](int i)->int{
        return i * i;
    }, 10);

    if(pool.commit_executor(ex).isSuccess){
        int result = ex->get();
        REQUIRE(result == 100);
    }
}


TEST_CASE("normal - run", "[ConcurrentThreadPool]"){
    muse::pool::ConcurrentThreadPool<1024> pool(4, ThreadPoolCloseStrategy::WaitAllTaskFinish, 1500ms);

    int isSuccess = true;
    try {
        for (int i = 0; i < 1200; ) {
            auto token1 = make_executor([&](int i)->void{
                std::this_thread::sleep_for( std::chrono::milliseconds(5));
                printf("logger: %d is running!\n", i);
            }, i);
            auto result = pool.commit_executor(token1);
            if (!result.isSuccess){
                std::this_thread::sleep_for( std::chrono::milliseconds(5));
            }else{
                i++;
            }
        }

        std::this_thread::sleep_for(4000ms);

        auto token1 = make_executor([](int i)->void{
            std::this_thread::sleep_for( std::chrono::milliseconds(5));
            printf("logger: %d is running!\n", i);
        }, 4000);

        pool.commit_executor(token1);

        std::this_thread::sleep_for(1600ms);

        for (int i = 4001; i < 4101; ) {
            auto token1 = make_executor([&](int i)->void{
                std::this_thread::sleep_for( std::chrono::milliseconds(5));
                printf("logger: %d is running!\n", i);
            }, i);
            auto result = pool.commit_executor(token1);
            if (!result.isSuccess){
                std::this_thread::sleep_for( std::chrono::milliseconds(5));
            }else{
                i++;
            }
        }
    }catch(...){
        isSuccess = false;
    }
    REQUIRE(isSuccess);
}