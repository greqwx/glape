# Build

You need a C99 compiler and CMake 3.16 or newer.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The binary is `build/glape`. On Windows it is under `build/Release`.
