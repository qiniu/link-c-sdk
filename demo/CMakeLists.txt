# ------------------------------------------------------------------------------
# link-c-sdk/demo CMakeLists.txt
# ------------------------------------------------------------------------------

execute_process(
  COMMAND git describe --abbrev=8 --dirty --always --tags
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_CODE_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions("-DCODE_VERSION=\"${GIT_CODE_VERSION}\"")

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

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/devices")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/tools")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ota")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/log")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/libs")
include_directories(${THIRD_INC_PATH})
link_directories(${THIRD_LIB_PATH})

message( ${CMAKE_CURRENT_SOURCE_DIR}/tools )

if (C_COMPILER_PREFIX)
    set( DEV_FILE ${THIRD_CODE_PATH} )
    add_definitions(-DENABLE_IPC)
else()
    set( DEV_FILE devices/sim_dev.c )
endif()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCE_LIST )
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/devices SOURCE_LIST )
aux_source_directory( ${CMAKE_CURRENT_SOURCE_DIR}/log SOURCE_LIST )
aux_source_directory( ${CMAKE_CURRENT_SOURCE_DIR}/tools SOURCE_LIST )
aux_source_directory( ${CMAKE_CURRENT_SOURCE_DIR}/ota SOURCE_LIST )

foreach(f IN LISTS SOURCE_LIST)
  get_filename_component(b ${f} NAME)
  set_source_files_properties(${f} PROPERTIES
    COMPILE_DEFINITIONS "__FILE_NAME__=\"${b}\"")
endforeach()

add_executable(demo-tsupload ${SOURCE_LIST} ${DEV_FILE})
add_dependencies(demo-tsupload lib)
target_compile_options(demo-tsupload PRIVATE -std=gnu99)
#libtsuploader
if(WITH_STATIC)
  target_link_libraries(demo-tsupload tsuploader_STATIC pthread m)
elseif(WITH_SHARED)
  target_link_libraries(demo-tsupload tsuploader_SHARED pthread m)
endif()
if(NOT APPLE)
    target_link_libraries(demo-tsupload rt)
endif()
#libghttp
if(WITH_STATIC)
  target_link_libraries(demo-tsupload ghttp_STATIC)
elseif(WITH_SHARED)
  target_link_libraries(demo-tsupload ghttp_SHARED)
endif()
#libmqtt
if(WITH_MQTT)
  if(WITH_STATIC)
    target_link_libraries(demo-tsupload mqtt_STATIC wolfmqtt pthread m)
  elseif(WITH_SHARED)
    target_link_libraries(demo-tsupload mqtt_SHARED wolfmqtt pthread m)
  endif()
endif()
#wolfssl
if(WITH_WOLFSSL)
  target_link_libraries(demo-tsupload wolfssl m)
endif()


if("${C_COMPILER_PREFIX}" STREQUAL "arm-hisiv300-linux")
  target_link_libraries( demo-tsupload -Wl,-Bstatic U5802YMain Schedule Upgrade Iniparser IOT HiGpioReg Video2 Audio2 aacenc aacdec Hicommon goscamqrtool Qrcode zbar jpeg curl Transport Interface GoscamISP Sysctrl  Record mp4lib Ntp GVAPDevice GVAPProtocal BroadcasDiscover Motor System Ulife Aliyun apr-1 aprutil-1 curl mxml  Tutk YhzlP2P AVAPIs RDTAPIs IOTCAPIs P2PTunnelAPIs json-c Upgrade isp mpi sns_sc2135 _hiaf _hiawb _hiae _cmoscfg _hidefog _iniparser dnvqe ive md upvqe VoiceEngine vo-aacenc  -Wl,-Bdynamic z pthread m rt dl -Wl,-Bstatic stdc++ Iniparser HiGpioReg Video2 Audio2 aacenc aacdec Hicommon goscamqrtool Qrcode zbar jpeg curl Transport Interface GoscamISP Sysctrl Record mp4lib Ntp GVAPDevice GVAPProtocal BroadcasDiscover Motor System Ulife Aliyun apr-1 aprutil-1 mxml Tutk AVAPIs RDTAPIs IOTCAPIs P2PTunnelAPIs json-c isp mpi sns_sc2135 _hiaf _hiawb _hiae  _cmoscfg _hidefog _iniparser dnvqe ive md upvqe VoiceEngine vo-aacenc  -Wl,-Bdynamic z pthread m rt dl  -Wl,-Bstatic stdc++ -Wl,-Bdynamic curl  aprutil-1 apr-1 )
elseif("${C_COMPILER_PREFIX}" STREQUAL "arm-linux-gnueabihf" )
  target_link_libraries( demo-tsupload devsdk tools pthread )
endif()


