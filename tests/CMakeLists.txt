# ------------------------------------------------------------------------------
# link-c-sdk/tests CMakeLists.txt
# ------------------------------------------------------------------------------

include_directories(${TOP_DIRS})
include_directories(${CONFIG_DIRS})
include_directories(${DEPENDS_DIRS})

include_directories(${LIBTSUPLOADER_INCLUDE_DIRS})
link_directories(${LIBTSUPLOADER_LIBRARIES})

include_directories(${LIBGHTTP_INCLUDE_DIRS})
link_directories(${LIBGHTTP_LIBRARIES})

if(WITH_MQTT)
  include_directories(${LIBTMQTT_INCLUDE_DIRS})
  link_directories(${LIBTMQTT_LIBRARIES})
  include_directories(${WOLFMQTT_INCLUDE_DIRS})
  link_directories(${WOLFMQTT_LIBRARIES})
endif()

if(WITH_WOLFSSL)
  include_directories(${WOLFSSL_INCLUDE_DIRS})
  link_directories(${WOLFSSL_LIBRARIES})
endif()

# ------------------------------------------------------------------------------
# Test libghttp
# ------------------------------------------------------------------------------

set(TEST_TARGET_1 "testghttp")
set(SRCS_${TEST_TARGET_1} libghttp_test.c)


# ------------------------------------------------------------------------------
# Test libmqtt
# ------------------------------------------------------------------------------

set(TEST_TARGET_2 "testmqtt")
set(SRCS_${TEST_TARGET_2} libmqtt_test.c)
aux_source_directory(${DEPENDS_DIRS}/log SRCS_${TEST_TARGET_2})

# ------------------------------------------------------------------------------
# Test libtsuploader
# ------------------------------------------------------------------------------

set(TEST_TARGET_3 "testupload")
set(SRCS_${TEST_TARGET_3} libtsuploader_test.c)
aux_source_directory(${DEPENDS_DIRS}/flag SRCS_${TEST_TARGET_3})

add_executable(${TEST_TARGET_3} ${SRCS_${TEST_TARGET_3}})
target_compile_options(${TEST_TARGET_3} PRIVATE -std=c99)
#libtsuploader
target_link_libraries(${TEST_TARGET_3} tsuploader pthread m)
#libghttp
target_link_libraries(${TEST_TARGET_3} ghttp)
#libmqtt
if(WITH_MQTT)
  target_link_libraries(${TEST_TARGET_3} mqtt wolfmqtt pthread)
endif()
#wolfssl
if(WITH_WOLFSSL)
  target_link_libraries(${TEST_TARGET_3} wolfssl m)
endif()

