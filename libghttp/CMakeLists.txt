cmake_minimum_required (VERSION 2.8)

project(libghttp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_definitions("-Wall -g")
if (USE_SSL)
    add_definitions(-DUSE_SSL)
    if (NOT SSL_DIR) 
	message(FATAL, "must give SSL_DIR when USE_SSL")
    endif()
    message("${USE_SSL} ${SSL_DIR}")
    set(SSL_INC_DIR "$SSL_INC_DIR")
    include_directories("${SSL_DIR}/include")
    include_directories("${SSL_DIR}/include/wolfssl")
    link_directories("${SSL_DIR}/lib")
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
)


add_executable(testghttp test.c qupload.c qupload.h)
if (USE_SSL)
    target_link_libraries(testghttp ghttp wolfssl m)
else()
    target_link_libraries(testghttp ghttp m)
endif()
