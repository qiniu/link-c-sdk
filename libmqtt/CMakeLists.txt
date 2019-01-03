cmake_minimum_required (VERSION 2.8)
project(libmqtt)

add_definitions("-DWITH_WOLFMQTT")
add_definitions("-Wall -g")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/wolfmqtt/")

if (WITH_MQTT)
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/lib")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfssl/output/${HOST}/include")
    link_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfMQTT/output/${HOST}/lib")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfMQTT/output/${HOST}/include")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wolfMQTT/output/${HOST}/include/wolfmqtt")
endif()

aux_source_directory(. LIB_MQTT)
aux_source_directory(../cJSON LIB_MQTT)
aux_source_directory(./wolfmqtt MQTT_MAP)

add_library(mqtt STATIC ${LIB_MQTT} ${MQTT_MAP})

# add_subdirectory(test)
