# ------------------------------------------------------------------------------
# CMakeLists.txt for Qiniu link-c-sdk
#
# copyright 2019(c) Shanghai Qiniu Information Technologies Co., Ltd.
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake" ${CMAKE_MODULE_PATH})
include(Utilities)
include(GNUInstallDirs)

project (link-c-sdk)

# ------------------------------------------------------------------------------
# Cross compile
# ------------------------------------------------------------------------------

if(CROSS_COMPILE AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
  set(CMAKE_C_COMPILER ${CROSS_COMPILE}-gcc)
  set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}-g++)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

# ------------------------------------------------------------------------------
# Get and Display infomation
# ------------------------------------------------------------------------------

message(STATUS "CMake version: " ${CMAKE_VERSION})
message(STATUS "CMake system name: " ${CMAKE_SYSTEM_NAME})
message(STATUS "C compiler: " ${CMAKE_C_COMPILER})
message(STATUS "CXX compiler: " ${CMAKE_CXX_COMPILER})

c_compiler_prefix(C_COMPILER_PREFIX)
if(C_COMPILER_PREFIX)
  message(STATUS "C compiler prefix: " ${C_COMPILER_PREFIX})
endif()

c_compiler_machine(C_COMPILER_MACHINE)
if(C_COMPILER_MACHINE)
  message(STATUS "C compiler machine infomation: " ${C_COMPILER_MACHINE})
endif()

# ------------------------------------------------------------------------------
# Build type
# ------------------------------------------------------------------------------

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}
  CACHE STRING "Choose the type of build: None Debug Release Coverage" FORCE)

if(CMAKE_BUILD_TYPE)
  message(STATUS "CMake build type: " ${CMAKE_BUILD_TYPE})
endif()

# Default C / CXX flags
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall ")
  set(CMAKE_CXX_FLAGS ${CMAKE_C_FLAGS})
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -ggdb -O0 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -Wall -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DDEFAULT_LOG_LEVEL=LOG_LEVEL_INFO")
set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})

set(CMAKE_C_FLAGS_COVERAGE "${CMAKE_C_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_COVERAGE ${CMAKE_C_FLAGS_COVERAGE})

option(TRACE_FUNCTION "Add Strace Information" OFF)
if(TRACE_FUNCTION)
    add_definitions(-DDEBUG_TRACE_FUNCTION)
    add_definitions("-funwind-tables -finstrument-functions")
endif()

# ------------------------------------------------------------------------------
# Version
# ------------------------------------------------------------------------------

set(LINK_C_SDK_VERSION_MAJOR 0)
set(LINK_C_SDK_VERSION_MINOR 1)
set(LINK_C_SDK_VERSION_PATCH 2)
set(LINK_C_SDK_RELEASE_VERSION ${LINK_C_SDK_VERSION_MAJOR}.${LINK_C_SDK_VERSION_MINOR}.${LINK_C_SDK_VERSION_PATCH})
message(STATUS "link-c-sdk release version: " ${LINK_C_SDK_RELEASE_VERSION})

get_commit_id(LINK_C_SDK_BUILD_VERSION)
if(LINK_C_SDK_BUILD_VERSION)
  message(STATUS "link-c-sdk build version: " ${LINK_C_SDK_BUILD_VERSION})
endif()

# ------------------------------------------------------------------------------
# target static and shared
# ------------------------------------------------------------------------------

option(WITH_STATIC "Build static library." ON)
option(WITH_SHARED "Build shared library." OFF)

if (NOT WITH_STATIC AND NOT WITH_SHARED)
  message(FATAL_ERROR "Need to choose static or shared build!")
endif()

# ------------------------------------------------------------------------------
# Test
# ------------------------------------------------------------------------------

option(ENABLE_TESTING "Build tests." ON)

if(ENABLE_TESTING)
  find_program(VALGRIND valgrind)
  if(VALGRIND)
    # Ctest configuration variables must be set BEFORE include(Ctest)
    set(MEMORYCHECK_COMMAND ${VALGRIND})
    # Check process children and give detail for each leak
    set(MEMORYCHECK_COMMAND_OPTIONS
      "${MEMORYCHECK_COMMAND_OPTIONS} --leak-check=yes --show-reachable=yes")
    set(MEMORYCHECK_COMMAND_OPTIONS
      "${MEMORYCHECK_COMMAND_OPTIONS} --trace-children=yes --track-fds=yes")
    # Set error code.
    set(MEMORYCHECK_COMMAND_OPTIONS
      "${MEMORYCHECK_COMMAND_OPTIONS} --error-exitcode=255")
    # Generate suppression hint.
    set(MEMORYCHECK_COMMAND_OPTIONS
      "${MEMORYCHECK_COMMAND_OPTIONS} --gen-suppressions=all")
    # Set suppression file
    set(MEMORYCHECK_COMMAND_OPTIONS
      "${MEMORYCHECK_COMMAND_OPTIONS} --suppressions=${CMAKE_CURRENT_LIST_DIR}/valgrind.supp")
  endif()
  include (CTest)
  enable_testing()
endif()

# ------------------------------------------------------------------------------
# Pack
# ------------------------------------------------------------------------------

set(CPACK_PACKAGE_NAME "link-c-sdk")
set(CPACK_PACKAGE_VENDOR "qiniu.com")
set(CPACK_PACKAGE_DESCRIPTION "copyright 2019(c) Shanghai Qiniu Information Technologies Co., Ltd.")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")

set(CPACK_PACKAGE_VERSION_MAJOR "${LINK_C_SDK_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${LINK_C_SDK_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${LINK_C_SDK_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_FULL "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

string(REGEX REPLACE
  "([A-Za-z0-9_\\.]+)\\-([A-Za-z0-9_\\.]+)\\-([A-Za-z0-9_\\.]+)" "\\1-\\2"
  C_COMPILE_ARCH_OS ${C_COMPILER_MACHINE})
set(CPACK_PACKAGE_FILE_NAME
  ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_FULL}-${C_COMPILE_ARCH_OS})
set(CPACK_GENERATOR "TGZ")
include (CPack)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------

option(WITH_MQTT "build with mqtt module" ON)
option(WITH_WOLFSSL "build with wolfssl third party module" OFF)
option(WITH_DEMO "build with demo" OFF)

# ------------------------------------------------------------------------------
# Configuration file
# ------------------------------------------------------------------------------

configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    @ONLY)

# ------------------------------------------------------------------------------
# Common path
# ------------------------------------------------------------------------------

set(CONFIG_DIRS ${CMAKE_CURRENT_BINARY_DIR})
set(DEPENDS_DIRS ${PROJECT_SOURCE_DIR}/deps)
set(TOP_DIRS ${PROJECT_SOURCE_DIR})

if(WITH_WOLFSSL)
  set(WOLFSSL_PREFIX ${PROJECT_BINARY_DIR}/third_party/wolfssl)
  set(WOLFSSL_INCLUDE_DIRS ${WOLFSSL_PREFIX}/include)
  set(WOLFSSL_LIBRARIES ${WOLFSSL_PREFIX}/lib)
endif()

if(WITH_MQTT)
  set(WOLFMQTT_PREFIX ${PROJECT_BINARY_DIR}/third_party/wolfmqtt)
  set(WOLFMQTT_INCLUDE_DIRS ${WOLFMQTT_PREFIX}/include)
  set(WOLFMQTT_LIBRARIES ${WOLFMQTT_PREFIX}/lib)
  set(LIBTMQTT_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/libtmqtt)
  set(LIBTMQTT_LIBRARIES ${PROJECT_BINARY_DIR}/libmqtt)
endif()

set(LIBGHTTP_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/libghttp)
set(LIBGHTTP_LIBRARIES ${PROJECT_BINARY_DIR}/libghttp)

set(LIBTSUPLOADER_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/libtsuploader)
set(LIBTSUPLOADER_LIBRARIES ${PROJECT_BINARY_DIR}/libtsuploader)

# ------------------------------------------------------------------------------
# Subdirectory
# ------------------------------------------------------------------------------

add_subdirectory(third_party)
if(WITH_MQTT)
  add_subdirectory(libmqtt)
endif()
add_subdirectory(libghttp)
add_subdirectory(libtsuploader)
add_subdirectory(tests)
if(WITH_DEMO)
	add_subdirectory(demo)
endif()

