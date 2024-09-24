# Building

These instructions assume that you have `cmake` installed, along with the necessary C++ compilers for your platform.

### Building and running on Windows
```
C:> mkdir build
C:> cd build
C:> cmake -S ..
C:> cd ..
C:> cmake --build build --config Release
C:> build\Release\lbavm-011
```

### Building and running on Linux
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release -S ..
$ cd ..
$ cmake --build build
$ build/lbavm-011
```
