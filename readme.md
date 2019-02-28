
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

## 交叉编译
- 修改toolchain.cmake,修改CMAKE\_C\_COMPILER和CMAKE\_CXX\_COMPILER为平台相关工具链
- mkdir build
- cd build
```
 cmake ../ \
    -DWITH_DEMO=ON \
    -DWITH_WOLFSSL=ON \
    -DWITH_MQTT=ON \
    -DCROSS_COMPILE=your-compile-toolchain-prefix \
    -DTHIRD_INC_PATH=/your/ipc/include/path \
    -DTHIRD_LIB_PATH=/your/ipc/lib/path \
    -DTHIRD_CODE_PATH=/your/ipc/code/path

 make
```

## 发布
- make package

