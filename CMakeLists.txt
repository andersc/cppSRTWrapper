cmake_minimum_required(VERSION 3.10)
project(cppSRTWrapper)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

include(ExternalProject)
ExternalProject_Add(project_srt
        GIT_REPOSITORY https://github.com/Haivision/srt.git
        UPDATE_COMMAND ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/srt
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/srt
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srt/configure
        GIT_SUBMODULES ""
        BUILD_COMMAND make srt_static
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )

add_library(srt STATIC IMPORTED)
set_property(TARGET srt PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/srt/libsrt.a)

find_package (Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/srt/)

add_library(srtnet STATIC SRTNet.cpp)
add_dependencies(srtnet project_srt)
target_link_libraries(srtnet srt ${OPENSSL_LIBRARIES} Threads::Threads)

add_executable(cppSRTWrapper main.cpp SRTNet.cpp)
add_dependencies(cppSRTWrapper srtnet)
target_link_libraries(cppSRTWrapper srtnet)
