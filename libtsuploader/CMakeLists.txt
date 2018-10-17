cmake_minimum_required (VERSION 2.8)

project(tsuploader)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/curl-7.61.1/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/c-sdk")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")

if (WITH_OPENSSL)
    message("WITH_OPENSSL")
    set(DISABLE_OPENSSL OFF)
    set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/openssl-1.1.0h")
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/openssl-1.1.0h")
else()
    message("WITHOUT_OPENSSL")
    set(DISABLE_OPENSSL ON)
    add_definitions(-DDISABLE_OPENSSL)
endif()

if (WITH_FFMPEG)
    message("WITH_FFMPEG")
else()
    message("WITHOUT_FFMPEG")
    add_definitions(-DUSE_OWN_TSMUX)
endif()

add_library(tsuploader STATIC
    resource.c
    resource.h
    uploader.h
    uploader.c
    base.h
    adts.h
    adts.c
    queue.c
    queue.h
    log.h
    log.c
    servertime.h
    servertime.c
    tsmuxuploader.c
    tsmuxuploader.h
    tsuploaderapi.c
    tsuploaderapi.h
    tsmux.h
    tsmux.c
    mpegts.c
    mpegts.h
    localkey.h
)

add_subdirectory(c-sdk)
