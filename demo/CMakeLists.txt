cmake_minimum_required (VERSION 2.8)

project(demo)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../libtsuploader")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../libtsuploader/c-sdk/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/libs")

if (WITH_OPENSSL)
    set(DISABLE_OPENSSL OFF)
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/lib")
else()
    set(DISABLE_OPENSSL ON)
    add_definitions(-DDISABLE_OPENSSL)
endif()

add_executable(demo-tsupload
    main.c
    main.h
    log2file.c
    log2file.h
    cfg.c
    cfg.h
    dbg.c
    dbg.h
    mymalloc.c
    mymalloc.h
    queue.c
    queue.h
    socket_logging.c
    socket_logging.h
    dev_core.c
    dev_core.h
    stream.c
    stream.h
    sim_dev.c
    ota.c
    md5.c
)

if(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS
    		"-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
	if(DISABLE_OPENSSL)
        	target_link_libraries(demo-tsupload tsuploader ${LIBFFMPEG} ghttp bz2 lzma iconv)
        else()
        	target_link_libraries(demo-tsupload tsuploader ${LIBFFMPEG} ghttp wolfssl bz2 lzma iconv)
        endif()
else()
    if(CMAKE_TOOLCHAIN_FILE)
        if(DISABLE_OPENSSL)
        	target_link_libraries( demo-tsupload tsuploader ${LIBFFMPEG} ghttp m pthread )
        else()
        	target_link_libraries( demo-tsupload tsuploader ${LIBFFMPEG} ghttp wolfssl m pthread )
        endif()
    else()
        if(DISABLE_OPENSSL)
        	target_link_libraries(demo-tsupload tsuploader ${LIBFFMPEG} ghttp m pthread)
        else()
        	target_link_libraries(demo-tsupload tsuploader ${LIBFFMPEG} ghttp wolfssl m pthread)
        endif()
    endif()
endif()

