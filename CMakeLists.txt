cmake_minimum_required(VERSION 3.10)
project(cppSRTWrapper)
set(CMAKE_CXX_STANDARD 17)

#If no build type is set then force Release
IF( NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
            "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
            FORCE)
ENDIF()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

include(ExternalProject)
ExternalProject_Add(project_srt
        GIT_REPOSITORY https://github.com/Haivision/srt.git
        UPDATE_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/srt
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/srt
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srt/configure --CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/srt --config ${CMAKE_BUILD_TYPE} --target srt_static
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )

ExternalProject_Add(project_srt_win
        GIT_REPOSITORY https://github.com/Haivision/srt.git
        UPDATE_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/srt
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/srt
        CONFIGURE_COMMAND cmake -DCMAKE_GENERATOR_PLATFORM=x64 -DENABLE_STDCXX_SYNC=ON ${CMAKE_CURRENT_SOURCE_DIR}/srt
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/srt --config ${CMAKE_BUILD_TYPE} --target srt_static
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )

add_library(srt STATIC IMPORTED)
IF (WIN32)
    set_property(TARGET srt PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/srt/${CMAKE_BUILD_TYPE}/srt_static.lib)
    add_dependencies(srt project_srt_win)
ELSE()
    set_property(TARGET srt PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/srt/libsrt.a)
    add_dependencies(srt project_srt)
ENDIF()

find_package (Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/srt/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/srt/common)

add_library(srtnet STATIC SRTNet.cpp)
target_link_libraries(srtnet srt ${OPENSSL_LIBRARIES} Threads::Threads)

add_executable(cppSRTWrapper main.cpp)
target_link_libraries(cppSRTWrapper srtnet)
