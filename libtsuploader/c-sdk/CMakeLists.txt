#设置CMAKE最低版本
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
#设置项目名称
SET(PROJECT_NAME qiniu)
#建立项目
PROJECT(${PROJECT_NAME})

if(NOT DISABLE_OPENSSL)
	if(NOT OPENSSL_ROOT_DIR)
        	message(FATAL_ERROR "shoud specify OPENSSL_ROOT_DIR")
	endif()
	message("openssl include:${OPENSSL_ROOT_DIR}/include")
	include_directories("${OPENSSL_ROOT_DIR}/include")
endif()
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/curl/output/${HOST}/include")

#添加C99支持
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -g")
#添加GNU99支持
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -g")

if(APPLE)
set(SOURCE_FILES
    b64/b64.c
    b64/urlsafe_b64.c
    cJSON/cJSON.c
    #qiniu/auth_mac.c
    qiniu/base.c
    qiniu/base_io.c
    qiniu/conf.c
    qiniu/fop.c
    qiniu/http.c
    qiniu/io.c
    qiniu/reader.c
    qiniu/resumable_io.c
    qiniu/rsf.c
)
else()
set(SOURCE_FILES
    b64/b64.c
    b64/urlsafe_b64.c
    cJSON/cJSON.c
    #qiniu/auth_mac.c
    qiniu/base.c
    qiniu/base_io.c
    qiniu/conf.c
    qiniu/fop.c
    qiniu/http.c
    qiniu/io.c
    qiniu/macro.h
    qiniu/reader.c
    qiniu/resumable_io.c
)
endif()

if(NOT DISABLE_OPENSSL)
        list(APPEND SOURCE_FILES qiniu/cdn.c qiniu/tm.c qiniu/auth_mac.c qiniu/rs.c qiniu/qetag.c)
endif()

ADD_LIBRARY(${PROJECT_NAME} STATIC ${SOURCE_FILES})
