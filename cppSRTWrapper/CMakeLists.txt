cmake_minimum_required(VERSION 3.10)
project(cppSRTWrapper)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
else ()
    # Search OpenSSL
    find_package(PkgConfig REQUIRED)
    pkg_search_module(OPENSSL REQUIRED openssl)

    if (OPENSSL_FOUND)
        include_directories(${OPENSSL_INCLUDE_DIRS})
        message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
    else ()
        # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
    endif ()
endif ()

set(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_FLAGS -pthread)
include_directories(${MY_SOURCE_DIR} ../srt/)
link_directories(../srt/)

add_library(srtnet STATIC SRTNet.cpp)
target_link_libraries(srtnet
        libsrt.a
        ${OPENSSL_LIBRARIES}
        )

add_executable(cppSRTWrapper
        main.cpp
        SRTNet.cpp
        )

target_link_libraries(cppSRTWrapper
        libsrt.a
        ${OPENSSL_LIBRARIES}
        )