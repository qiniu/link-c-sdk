
# 编译

## x86
- mkdir build
- cd build
```
 cmake ../ \
    -DWITH_DEMO=ON \
    -DWITH_WOLFSSL=ON \
    -DWITH_MQTT=ON
 make
```

## 嵌入式
- 修改toolchain.cmake,修改CMAKE\_C\_COMPILER和CMAKE\_CXX\_COMPILER为平台相关工具链
- mkdir build
- cd build
```
 cmake ../ \
    -DWITH_DEMO=ON \
    -DWITH_WOLFSSL=ON \
    -DWITH_MQTT=ON \
    -DTHIRD_INC_PATH=/your/ipc/include/path \
    -DTHIRD_LIB_PATH=/your/ipc/lib/path \
    -DTHIRD_CODE_PATH=/your/ipc/code/path \
    -DCMAKE_TOOLCHAIN_FILE=../CMake/toolchain.cmake
 make
```

