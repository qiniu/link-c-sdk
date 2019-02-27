cmake_minimum_required(VERSION 2.8)

project(sample)
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
#添加头文件路径

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../libtsuploader)


configure_file (
        "${PROJECT_SOURCE_DIR}/sample_ver.h.in"
        "${PROJECT_SOURCE_DIR}/sample_ver.h"
)

#获取库文件
#find_library(LINK_TSUPLOAD_LIBRARY tsuploader PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)
find_library(PTHREAD_LIBRARY NAMES pthread)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build-x86/libtsuploader)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wolfssl/output/x86_64-linux/lib)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wolfMQTT/output/x86_64-linux/lib)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build-x86/libmqtt)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../build-x86/libghttp)

set(LINK_TSUPLOAD_LIBRARY tsuploader)

add_definitions("-g")
option(BUILD_SAMPLE "build sample" ON)
IF (NOT BUILD_SAMPLE)
    set(SAMPLE_SRC sample.c)
    set(SAMPLE_BIN sample)
    add_executable(${SAMPLE_BIN} ${SAMPLE_SRC})

    target_link_libraries(${SAMPLE_BIN} tsuploader mqtt ghttp wolfmqtt wolfssl pthread m)
ENDIF()

