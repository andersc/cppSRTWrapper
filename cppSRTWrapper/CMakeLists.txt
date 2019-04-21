cmake_minimum_required(VERSION 3.13)
project(cppSRTWrapper)

set(CMAKE_CXX_STANDARD 14)

include_directories( ${MY_SOURCE_DIR} ../srt/ )
link_directories(../srt/)

file(GLOB SOURCES
        ./*.h
        ./*.cpp
        )


add_executable(cppSRTWrapper
        main.cpp)

target_link_libraries(cppSRTWrapper libsrt.a)