cmake_minimum_required(VERSION 3.10)
project(RPC_FS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_library(client_lib src/client_rpc.cpp)

add_executable(client_app src/client_app.cpp)
target_link_libraries(client_app client_lib)

add_executable(server_app src/server_rpc.cpp src/server_app.cpp)