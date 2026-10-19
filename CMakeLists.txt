cmake_minimum_required(VERSION 3.20)
project(CodeA LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(hello src/main.cpp)

option(CODEA_BUILD_EXERCISES "Build the chapter exercise programs" ON)
if(CODEA_BUILD_EXERCISES)
    add_subdirectory(exercises)
endif()
