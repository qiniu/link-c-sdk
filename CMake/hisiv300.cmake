# ------------------------------------------------------------------------------
# hisiv300.cmake
# ------------------------------------------------------------------------------

# this is required
set(CMAKE_SYSTEM_NAME Linux)

# specify the cross compiler
set(TOOLCHAIN_PREFIX arm-hisiv300-linux)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
