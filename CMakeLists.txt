cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(muse-rpc
        VERSION         0.0.1
        DESCRIPTION     "A Simple RPC Application Framework Based on Connection UDP"
        HOMEPAGE_URL    "https://github.com/muse"
        LANGUAGES       C CXX
)

include_directories(include)
include_directories(dependency)
include_directories(dependency/catch2)
include_directories(dependency/concurrent_queue)

add_subdirectory(src)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED  ON)

add_library(timer SHARED
        ${CMAKE_SOURCE_DIR}/include/timer/timer_wheel.cpp
        ${CMAKE_SOURCE_DIR}/include/timer/timer_tree.cpp
)

add_library(museThreads SHARED
        ${CMAKE_SOURCE_DIR}/include/thread_pool/executor.cpp
        ${CMAKE_SOURCE_DIR}/include/thread_pool/conf.cpp
        ${CMAKE_SOURCE_DIR}/include/thread_pool/pool.cpp
        ${CMAKE_SOURCE_DIR}/include/thread_pool/concurrent_pool.cpp
)

add_library(rpc SHARED
        ${CMAKE_SOURCE_DIR}/include/rpc/protocol/protocol.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/reactor.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/request.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/sub_reactor.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/reactor_exception.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/reactor_pool.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/response.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/registry.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/rpc_response_header.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/middleware_channel.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/route_service.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/zlib_service.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/peer.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/synchronous_registry.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/global_entry.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/client/response_data.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/client/invoker.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/client/client.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/client/invoker_exception.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/client/transmitter.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/client/outcome.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/client/transmitter_event.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/client/global_id.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/memory/conf.cpp
)

target_link_libraries(rpc
        PUBLIC serializer museThreads timer z
)

#查找需要的库在不在
find_library(Zlib_Find_Result z
        PATHS /usr/local/lib
        REQUIRED
)

if (Zlib_Find_Result)
    MESSAGE("-- find zlib in ${Zlib_Find_Result} ----- OK!")
else ()
    MESSAGE(FATAL_ERROR "-- didn't find zlib ----- ERROR!")
endif ()

add_executable(muse main.cpp)
add_executable(client client.cpp)
add_executable(call bind.cpp)

target_link_libraries(call
        PUBLIC z
)

target_link_libraries(muse
        PUBLIC rpc
)

target_link_libraries(call PUBLIC serializer)

target_link_libraries(client
        PUBLIC rpc
)

#add thread only linux gcc
set(CMAKE_CXX_FLAGS "${CAMKE_CXX_FLAGS} -std=c++17 -pthread")

# 启动单元测试
enable_testing()

# 进入单元测试
add_subdirectory(tests)