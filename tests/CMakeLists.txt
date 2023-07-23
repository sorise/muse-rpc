cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

add_executable(poolUnit poolTest.cpp)
add_executable(protocol_conf_ut rpc/protocol_conf_ut.cpp)
add_executable(server_request_ut rpc/server_request_ut.cpp)

target_link_libraries(protocol_conf_ut
        PUBLIC rpc
)

target_link_libraries(server_request_ut
        PUBLIC rpc
)

add_test( NAME "poolUnit" COMMAND poolUnit )

add_test( NAME "protocol_conf_ut" COMMAND protocol_conf_ut )

add_test( NAME "server_request_ut" COMMAND server_request_ut )
