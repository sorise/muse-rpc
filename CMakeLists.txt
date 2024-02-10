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
include_directories(dependency/spdlog-1.13.0/include)
include_directories(dependency/concurrent_queue)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED  ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wall -fPIC)
endif()

#add thread only linux gcc
set(CMAKE_CXX_FLAGS "${CAMKE_CXX_FLAGS} -std=c++17 -pthread")

#解决 visual studio 编码问题,暂时不支持 windows
if (WIN32)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif ()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    SET(ZLIB_LIBRARY_PATH  "/usr/local/lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")

else()

endif()

add_subdirectory(dependency/spdlog-1.13.0)

add_library(rpc SHARED
    include/rpc/protocol/protocol.cpp
    include/rpc/server/reactor.cpp
    include/rpc/server/request.cpp
    include/rpc/server/sub_reactor.cpp
    include/rpc/server/reactor_exception.cpp
    include/rpc/server/reactor_pool.cpp
    include/rpc/server/response.cpp
    include/rpc/server/registry.cpp
    include/rpc/server/rpc_response_header.cpp
    include/rpc/server/middleware_channel.cpp
    include/rpc/server/route_service.cpp
    include/rpc/server/zlib_service.cpp
    include/rpc/server/peer.cpp
    include/rpc/server/synchronous_registry.cpp
    include/rpc/server/global_entry.cpp
    include/rpc/server/transmitter_link_reactor.cpp
    include/rpc/client/response_data.cpp
    include/rpc/client/invoker.cpp
    include/rpc/client/client.cpp
    include/rpc/client/invoker_exception.cpp
    include/rpc/client/transmitter.cpp
    include/rpc/client/outcome.cpp
    include/rpc/client/transmitter_event.cpp
    include/rpc/client/global_id.cpp
    include/rpc/memory/conf.cpp
    include/rpc/rpc.cpp
    include/thread_pool/executor.cpp
    include/thread_pool/conf.cpp
    include/thread_pool/pool.cpp
    include/thread_pool/concurrent_pool.cpp
    include/serializer/binarySerializer.cpp
    include/serializer/util.cpp
    include/serializer/binaryDeserializeView.cpp
    include/timer/timer_wheel.cpp
    include/timer/timer_tree.cpp
)

target_link_libraries(
    rpc PUBLIC z spdlog
)

#查找需要的库在不在
find_library(MUSE_RPC_Zlib z
    PATHS "${ZLIB_LIBRARY_PATH}"
    REQUIRED
)

if (MUSE_RPC_Zlib)
    MESSAGE("-- find zlib in ${MUSE_RPC_Zlib} ----- OK!")
else ()
    MESSAGE(FATAL_ERROR "-- didn't find zlib ----- ERROR!")
endif ()

add_executable(muse main.cpp)
add_executable(client client.cpp)

target_link_libraries(muse
    PUBLIC rpc
)

target_link_libraries(client
    PUBLIC rpc
)

# 启动单元测试
enable_testing()
# 进入单元测试
add_subdirectory(tests)