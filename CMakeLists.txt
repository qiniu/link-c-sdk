
cmake_minimum_required (VERSION 2.8)

project (link-c-sdk)

message( "CMAKE_C_COMPILER is " ${CMAKE_C_COMPILER} )
set( COMPILER 0 )
string( REGEX MATCH "[^/]+$" COMPILER ${CMAKE_C_COMPILER})
message( "COMPILER is " ${COMPILER} )
#string( LENGTH ${COMPILER} LEN )
set( POS 0 )
if ( NOT ${COMPILER} STREQUAL cc )
    string( FIND ${COMPILER} -gcc POS )
endif()
message("POS IS " ${POS} )

string( FIND ${CMAKE_C_COMPILER} "CommandLineTools" MACPOS )
string( FIND ${CMAKE_C_COMPILER} "/usr/bin/" LINUXPOS )
message( "POS IS " ${POS} )
set( HOST "x86_64-linux" )
if ( ${POS} GREATER 0 )
    string( SUBSTRING ${COMPILER} 0 ${POS} HOST )
# if the cc tool path contain "CommandLineTools", we guress it is in mac os
elseif( ${MACPOS} GREATER 0 )
    set( HOST "x86_64-apple-darwin" )
# if the cc tool path contail "/usr/bin/", we guess it is in linux os
elseif( ${LINUXPOS} GREATER 0 )
    set ( HOST "x86_64-linux")
endif()
if (APPLE) 
    set( HOST "x86_64-apple-darwin" )
endif()
message( "HOST is " ${HOST} )

add_custom_target(all-clean
        COMMAND echo "CLEAN WOLFSSL"
        COMMAND cd ${CMAKE_HOME_DIRECTORY}/third_party/wolfssl && git clean -dxf
    )

SET(GCC_COVERAGE_COMPILE_FLAGS "-Wno-unused")
if (WITH_DEBUG)
    message("WITH_DEBUG")
    add_definitions("-Wall -g -fstack-protector")
endif()

if(TRACE_FUNCTION)
    add_definitions(-DDEBUG_TRACE_FUNCTION)
    add_definitions("-funwind-tables -finstrument-functions")
endif()

execute_process(
  COMMAND git describe --abbrev=8 --dirty --always --tags
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_CODE_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions("-DCODE_VERSION=\"${GIT_CODE_VERSION}\"")

add_subdirectory(third_party)
add_subdirectory(libghttp)
add_subdirectory(libtsuploader)
if (WITH_WOLFSSL AND WITH_MQTT)
	add_subdirectory(libmqtt)
endif()
add_subdirectory(tests/functional)
add_subdirectory(tests/unit)
add_subdirectory(monster)
if (WITH_DEMO)
	add_subdirectory(demo)
endif()

