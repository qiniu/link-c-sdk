cmake_minimum_required (VERSION 2.8)

project (link-c-sdk)

SET(GCC_COVERAGE_COMPILE_FLAGS "-Wno-unused")
if (WITH_DEBUG)
    message("WITH_DEBUG")
    add_definitions("-Wall -g")
endif()

add_subdirectory(third_party)
add_subdirectory(libtsuploader)
add_subdirectory(demo)
