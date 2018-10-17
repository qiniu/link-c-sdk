
一、编译
1. linux平台
    * mkdir build-linux
    * cd build-linux
    * cmake ..
    * make
2. mac 平台
    * mkdir build-mac
    * cd build-mac
    * cmake ..
    * make
3. 嵌入式平台
  3.1 ARM
    * 修改toolchain.cmake,修改CMAKE_C_COMPILER和CMAKE_CXX_COMPILER为平台相关工具链
    * mkdir build-arm
    * cd build-arm
    * cmake ..
    * make
  3.2 MIPS
    * 修改toolchain.cmake,修改CMAKE_C_COMPILER和CMAKE_CXX_COMPILER为平台相关工具链
    * mkdir build-mips
    * cd build-mips
    * cmake ..
    * make
4.clean
   * 默认curl第三方库编译一次之后不会再编译，如果需要重新编译curl库，需要执行make all-clean

