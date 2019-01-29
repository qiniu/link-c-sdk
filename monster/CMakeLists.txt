cmake_minimum_required (VERSION 2.8)

project(monster)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../b64")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../libmqtt")

link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/lib")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfMQTT/output/${HOST}/lib")
include_directories(${THIRD_INC_PATH})
link_directories(${THIRD_LIB_PATH})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../b64 B64_SOURCE_LIST)
set(SOURCE_LIST device.c common.c connect.c)		
add_executable(demo-mqtt 	${SOURCE_LIST} ${B64_SOURCE_LIST})	
target_link_libraries(demo-mqtt mqtt wolfmqtt wolfssl m pthread)


