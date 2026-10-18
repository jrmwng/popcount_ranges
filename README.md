# popcount_ranges

`popcount_ranges` is a header-only C++23 library for working with fixed-popcount
integer ranges, wide integers, and SIMD-assisted search.

## Requirements

- A C++23 compiler.
- An x86 CPU with AVX2 support when using the CMake target. The target propagates
  the AVX2 compiler option to consumers.
- CMake 3.20 or newer.

The `data_par` SIMD facility has no scalar backend. Including
`data_par/simd_native.hpp` without an AVX2 or SSE4.1/4.2 compiler target produces
a compile-time error. The normal CMake target builds for AVX2.

## Build and install

```text
cmake -S . -B build -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=OFF
cmake --build build
cmake --install build --prefix <install-prefix>
```

To run the test suite, enable testing when configuring and run CTest:

```text
cmake -S . -B build -DBUILD_TESTING=ON -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

## Use from another CMake project

After installation, find the package and link the exported target:

```cmake
find_package(popcount_ranges CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE jrmwng::popcount_ranges)
```

The target supplies the installed headers, the C++23 requirement, and the AVX2
compiler option. For a build-tree dependency, add this repository as a
subdirectory and link the same `jrmwng::popcount_ranges` target.
The [consumer example](examples/consumer) can be configured with
`-DCMAKE_PREFIX_PATH=<install-prefix>` to verify an installation.

Include the API headers directly, for example:

```cpp
#include <popcount_ranges/set.hpp>
```

See [benchmarks/README.md](benchmarks/README.md) for benchmark targets and usage.
