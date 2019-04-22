cmake_minimum_required(VERSION 3.13)
project(cppSRTWrapper)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -v -stdlib=libc++")

include_directories( ${MY_SOURCE_DIR} ../srt/ )
link_directories(../srt/)

add_executable(cppSRTWrapper
        main.cpp SRTNet.cpp SRTGlobalHandler.cpp)

target_link_libraries(cppSRTWrapper
        libsrt.a
        ${CMAKE_DL_LIBS}
        ${CMAKE_THREAD_LIBS_INIT}
        )