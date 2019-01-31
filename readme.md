
# 编译
## 初始化第三方库
- git submodule update --init --recursive

## x86
- mkdir build
- cd build
```   
 cmake ../ \
    -DWITH_DEMO=yes \
    -DWITH_WOLFSSL=yes \
    -DWITH_MQTT=yes 
  make
```

## 嵌入式
- 修改toolchain.cmake,修改CMAKE_C_COMPILER和CMAKE_CXX_COMPILER为平台相关工具链
- mkdir build
- cd build
```   
 cmake ../ \
    -DWITH_DEMO=yes \
    -DWITH_WOLFSSL=yes \
    -DWITH_MQTT=yes \
    -DTHIRD_INC_PATH=/your/ipc/include/path \
    -DTHIRD_LIB_PATH=/your/ipc/lib/path \
    -DTHIRD_CODE_PATH=/your/ipc/code/path \
    -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake
  make
```

## clean
- 默认curl第三方库编译一次之后不会再编译，如果需要重新编译curl库，需要执行make all-clean

