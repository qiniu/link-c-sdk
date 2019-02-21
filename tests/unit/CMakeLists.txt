cmake_minimum_required (VERSION 2.8)

project(link_unit_test)

add_definitions("-Wall -g")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../libtsuploader")
message( "line-8 HOST is " ${HOST} )
message( "line-8 HOST is " ${CFLAGS} )

link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wolfssl/output/${HOST}/lib")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wolfMQTT/output/${HOST}/lib")
link_directories("${CMAKE_CURRENT_BINARY_DIR}/../../libmqtt")

add_executable(link_unit_test
    CuTest.c
    link_config.c
    link_test_utils.c
    test_all.c
    test_link_queue.c
    test_link_api.c
)

target_link_libraries(link_unit_test tsuploader ghttp mqtt wolfmqtt wolfssl rt m pthread)

