cmake_minimum_required (VERSION 2.8)

project(testupload)


include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../libtsuploader")
message( "line-8 HOST is " ${HOST} )
message( "line-8 HOST is " ${CFLAGS} )


if (TEST_WITH_FFMPEG)
        message("TEST_WITH_FFMPEG")
        set(LIBFFMPEG avformat avcodec swresample avutil)
        add_definitions(-DTEST_WITH_FFMPEG)
else()
        message("TEST_WITHOUT_FFMPEG")
endif()

if (WITH_OPENSSL)
    message("test WITH_OPENSSL")
    set(DISABLE_OPENSSL OFF)
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wolfssl/output/${HOST}/lib")
elseif (WITH_WOLFSSL)
    message("test WITH_WOLFSSL")
    set(DISABLE_OPENSSL OFF)
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wolfssl/output/${HOST}/lib")
else()
    message("test WITHOUT_OPENSSL")
    set(DISABLE_OPENSSL ON)
    add_definitions(-DDISABLE_OPENSSL)
endif()

if (WITH_MQTT)
	link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wolfMQTT/output/${HOST}/lib")
	link_directories("${CMAKE_CURRENT_BINARY_DIR}/../../libmqtt")
endif()

add_executable(testupload
    test.c
    flag.h
    flag.c
)

if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS
            "-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
    if(DISABLE_OPENSSL)
            target_link_libraries(testupload tsuploader ${LIBFFMPEG} ghttp bz2 lzma iconv)
        else()
            target_link_libraries(testupload tsuploader ${LIBFFMPEG} ghttp mqtt wolfmqtt wolfssl bz2 lzma iconv)
        endif()
else()
    if(CMAKE_TOOLCHAIN_FILE)
        if(DISABLE_OPENSSL)
            target_link_libraries(testupload tsuploader ${LIBFFMPEG} ghttp m pthread )
        else()
            target_link_libraries(testupload tsuploader ${LIBFFMPEG} ghttp mqtt wolfmqtt wolfssl m pthread )
        endif()
    else()
        if(DISABLE_OPENSSL)
            target_link_libraries(testupload tsuploader ${LIBFFMPEG} ghttp m pthread)
        else()
            target_link_libraries(testupload tsuploader ${LIBFFMPEG} ghttp mqtt wolfmqtt wolfssl m pthread)
        endif()
    endif()
endif()

