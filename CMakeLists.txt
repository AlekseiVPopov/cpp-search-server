cmake_minimum_required(VERSION 3.23)
project(cpp_search_server)

set(CMAKE_CXX_STANDARD 17)

add_executable(cpp_search_server
        search-server/main.cpp)
