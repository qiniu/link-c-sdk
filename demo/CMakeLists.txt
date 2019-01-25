cmake_minimum_required (VERSION 2.8)

project(demo)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../libtsuploader")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../libtsuploader/c-sdk/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/libs")
include_directories(${THIRD_INC_PATH})
link_directories(${THIRD_LIB_PATH})

if (WITH_OPENSSL)
    set(DISABLE_OPENSSL OFF)
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/lib")
elseif (WITH_WOLFSSL)
    set(DISABLE_OPENSSL OFF)
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/lib")
else()
    set(DISABLE_OPENSSL ON)
    add_definitions(-DDISABLE_OPENSSL)
endif()

if (WITH_MQTT)
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfMQTT/output/${HOST}/lib")
    link_directories("${CMAKE_CURRENT_BINARY_DIR}/../libmqtt")
endif()

if (CMAKE_TOOLCHAIN_FILE)
    set( DEV_FILE ipc_dev.c )
    add_definitions(-DENABLE_IPC)
else()
    set( DEV_FILE sim_dev.c )
endif()

set(SOURCE_LIST main.c main.h log2file.c log2file.h cfg.c cfg.h dbg.c dbg.h mymalloc.c 
			mymalloc.h queue.c queue.h log2tcp.c log2tcp.h dev_core.c dev_core.h stream.c 
			stream.h ota.c md5.c func_trace.c)

if (MODULE_COLLECTOR)
	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../libmqtt")
	set(SOURCE_LIST ${SOURCE_LIST} collector.c connect_mqtt.c log_mqtt.c)
endif()

if (WITH_MQTT)
	link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfMQTT/output/${HOST}/lib")
	link_directories("${CMAKE_CURRENT_BINARY_DIR}/../libmqtt")
endif()
			
add_executable(demo-tsupload 	${SOURCE_LIST} ${DEV_FILE})	

if(APPLE)
	set(CMAKE_EXE_LINKER_FLAGS
        "-framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
    if(DISABLE_OPENSSL)
            target_link_libraries(demo-tsupload tsuploader ${LIBFFMPEG} ghttp bz2 lzma iconv)
        else()
            target_link_libraries(demo-tsupload tsuploader ${LIBFFMPEG} ghttp mqtt wolfmqtt wolfssl bz2 lzma iconv)
        endif()
else()
    if(CMAKE_TOOLCHAIN_FILE)
        if(DISABLE_OPENSSL)
            target_link_libraries( demo-tsupload tsuploader ${LIBFFMPEG} ghttp m devsdk tools pthread )
        else()
            target_link_libraries( demo-tsupload tsuploader ${LIBFFMPEG} ghttp mqtt wolfmqtt wolfssl m devsdk tools pthread )
        endif()
    else()
        if(DISABLE_OPENSSL)
            target_link_libraries(demo-tsupload tsuploader ${LIBFFMPEG} ghttp m pthread)
        else()
            target_link_libraries(demo-tsupload tsuploader ${LIBFFMPEG} ghttp mqtt wolfmqtt wolfssl m pthread)
        endif()
    endif()
endif()

