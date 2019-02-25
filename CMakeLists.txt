
cmake_minimum_required (VERSION 2.8)

project (link-c-sdk)

# Returns the ${CMAKE_C_COMPILER} prefix
function(c_compiler_prefix output_prefix)
  string(REGEX MATCH "[^/]+$" compiler_basename ${CMAKE_C_COMPILER})
  string(REGEX MATCH "[^-]+$" compiler_id ${compiler_basename})
  if(NOT ${compiler_basename} STREQUAL ${compiler_id})
    string(REPLACE "-${compiler_id}" "" compiler_prefix ${compiler_basename})
  endif()
  set(${output_prefix} ${compiler_prefix} PARENT_SCOPE)
endfunction()

c_compiler_prefix(C_COMPILER_PREFIX)
set(HOST ${C_COMPILER_PREFIX})
message( "HOST is " ${HOST} )

# Returns the ${CMAKE_C_COMPILER} machine infomation
function(c_compiler_machine output_machine)
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} -dumpmachine
    OUTPUT_VARIABLE compiler_machine
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  set(${output_machine} ${compiler_machine} PARENT_SCOPE)
endfunction()

c_compiler_machine(C_COMPILER_MACHINE)
if(C_COMPILER_MACHINE)
  message(STATUS "C compiler machine infomation: " ${C_COMPILER_MACHINE})
endif()

add_custom_target(all-clean
        COMMAND echo "CLEAN WOLFSSL"
        COMMAND cd ${CMAKE_HOME_DIRECTORY}/third_party/wolfssl && git clean -dxf
    )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D__FILE_NAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

SET(GCC_COVERAGE_COMPILE_FLAGS "-Wno-unused")
if (WITH_DEBUG)
    message("WITH_DEBUG")
    add_definitions("-Wall -g -fstack-protector-all")
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

