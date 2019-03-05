
# 编译

### x86

```
mkdir build
cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=/your/install/path/
make
make install
```

### 交叉编译

```
mkdir build
cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=/your/install/path/ -DCROSS_COMPILE=/you/toolchain/prefix/
make
make install
```

## 发布
- make package

