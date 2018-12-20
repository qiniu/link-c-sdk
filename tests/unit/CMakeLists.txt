cmake_minimum_required (VERSION 2.8)

project(link_unit_test)


include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../libtsuploader")
message( "line-8 HOST is " ${HOST} )
message( "line-8 HOST is " ${CFLAGS} )


if (WITH_OPENSSL)
    message("test WITH_OPENSSL")
    set(DISABLE_OPENSSL OFF)
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wolfssl/output/${HOST}/lib")
else()
    message("test WITHOUT_OPENSSL")
    set(DISABLE_OPENSSL ON)
    add_definitions(-DDISABLE_OPENSSL)
endif()

add_executable(link_unit_test
    CuTest.c
    link_config.c
    link_test_utils.c
    test_all.c
    test_link_queue.c
)

if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS
            "-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
    if(DISABLE_OPENSSL)
            target_link_libraries(link_unit_test tsuploader ${LIBFFMPEG} ghttp bz2 lzma iconv)
        else()
            target_link_libraries(link_unit_test tsuploader ${LIBFFMPEG} ghttp wolfssl bz2 lzma iconv)
        endif()
else()
    if(CMAKE_TOOLCHAIN_FILE)
        if(DISABLE_OPENSSL)
            target_link_libraries(link_unit_test tsuploader ${LIBFFMPEG} ghttp m pthread )
        else()
            target_link_libraries(link_unit_test tsuploader ${LIBFFMPEG} ghttp wolfssl m pthread )
        endif()
    else()
        if(DISABLE_OPENSSL)
            target_link_libraries(link_unit_test tsuploader ${LIBFFMPEG} ghttp m pthread)
        else()
            target_link_libraries(link_unit_test tsuploader ${LIBFFMPEG} ghttp wolfssl m pthread)
        endif()
    endif()
endif()
