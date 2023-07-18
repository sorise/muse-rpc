cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(muse
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

add_library(rpc SHARED
        ${CMAKE_SOURCE_DIR}/include/rpc/protocol/conf.cpp
        ${CMAKE_SOURCE_DIR}/include/rpc/server/reactor.cpp
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

target_link_libraries(muse
        PUBLIC serializer
        PUBLIC z
        PUBLIC rpc
)


target_link_libraries(client
        PUBLIC serializer
        PUBLIC rpc
)

# 启动单元测试
enable_testing()
# 进入单元测试
add_subdirectory(tests)