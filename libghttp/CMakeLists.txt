cmake_minimum_required (VERSION 2.8)

project(libghttp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_definitions("-Wall -g")
if (WITH_WOLFSSL)
    add_definitions(-DUSE_WOLFSSL)

    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/lib")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/include")
endif()

if (WITH_OPENSSL)
    add_definitions(-DUSE_OPENSSL)

    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/lib")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/include")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/include/wolfssl")
endif()

add_library(ghttp STATIC
	ghttp.c
	ghttp.h
	ghttp_constants.h
	http_base64.c
	http_base64.h
	http_date.c
	http_date.h
	http_global.h
	http_hdrs.c
	http_hdrs.h
	http_req.c
	http_req.h
	http_resp.c
	http_resp.h
	http_trans.c
	http_trans.h
	http_uri.c
	http_uri.h
	qupload.h
	qupload.c
	timeoutconn.h
	timeoutconn.c
)


add_executable(testghttp test.c qupload.c qupload.h)
if (WITH_OPENSSL)
    target_link_libraries(testghttp ghttp wolfssl m)
elseif (WITH_WOLFSSL)
	target_link_libraries(testghttp ghttp wolfssl m)
else()
    target_link_libraries(testghttp ghttp m)
endif()
