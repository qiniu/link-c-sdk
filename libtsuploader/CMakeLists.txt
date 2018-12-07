cmake_minimum_required (VERSION 2.8)

project(tsuploader)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../libghttp")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")

if (WITH_OPENSSL)
    message("WITH_OPENSSL")
    set(DISABLE_OPENSSL OFF)
    set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}")
    link_directories("${OPENSSL_ROOT_DIR}/lib")
    include_directories("${OPENSSL_ROOT_DIR}/include")
    include_directories("${OPENSSL_ROOT_DIR}/include/wolfssl")
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
    tsuploader.h
    tsuploader.c
    base.h
    adts.h
    adts.c
    queue.c
    queue.h
    kmp.h
    kmp.c
    log.h
    log.c
    servertime.h
    servertime.c
    tsmuxuploader.c
    tsmuxuploader.h
    tsuploaderapi.c
    uploader.h
    httptools.h
    httptools.c
    tsmux.h
    tsmux.c
    mpegts.c
    mpegts.h
    fixjson.h
    fixjson.c
    picuploader.h
    picuploader.c
    segmentmgr.c
    segmentmgr.h
    security.h
    security.c
    b64/b64.c
    b64/b64.h
    b64/urlsafe_b64.c
    b64/urlsafe_b64.h
    cJSON/cJSON.h
    cJSON/cJSON.c
)
