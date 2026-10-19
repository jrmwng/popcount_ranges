# popcount_ranges

`popcount_ranges` is a header-only C++23 library for working with fixed-popcount
integer ranges, wide integers, and SIMD-assisted search. An optional C++26
backend uses the standard `<simd>` header for `lower_bound`.

## Requirements

- A C++23 compiler.
- An x86 CPU with AVX2 support when using the CMake target. The target propagates
  the AVX2 compiler option to consumers.
- CMake 3.20 or newer.
- When using the standard SIMD backend, a C++26 compiler and standard library
  providing `<simd>`.

By default, `lower_bound` uses the existing `data_par` SIMD implementation. The
`data_par` SIMD facility has no scalar backend. Including
`data_par/simd_native.hpp` without an AVX2 or SSE4.1/4.2 compiler target produces
a compile-time error. The normal CMake target builds for AVX2.

The CMake option `POPCOUNT_RANGES_SIMD` selects the C++26 `<simd>` backend for
`lower_bound` and raises the target language requirement to C++26:

```text
cmake -S . -B build -DPOPCOUNT_RANGES_SIMD=ON
```

The corresponding preprocessor macro is numeric: define
`POPCOUNT_RANGES_SIMD=1` to select `<simd>` or `0` (the default) to select the
existing `data_par/simd_*.hpp` implementation. The custom
`data_par/simd_range.hpp` range and `simd_short_range` interfaces are retained
for both backends; the header maps their vector, mask, load, and mask-count
operations to the selected implementation rather than replacing these range
types with standard-library ranges.

`simd_range_traits<T, Mask, Abi>`, `simd_long_range<T, Mask, Abi>`, and
`simd_short_range<T, Mask, Abi>` accept an optional mask type before the
optional ABI type. The ABI is inferred from a SIMD mask's `abi_type`; masks
without ABI metadata, such as `std::bitset`, use the backend's default ABI. The
third template argument remains available to override it. The default mask
remains the backend's mask type. Use `jrmwng::data_par::simd_bitset_mask<T>` to select a `std::bitset` sized to
the backend's SIMD width; for example,
`jrmwng::data_par::simd_range_traits<T, jrmwng::data_par::simd_bitset_mask<T>>`
lets the `lower_bound` SIMD path combine range-validity and comparison masks
directly. A custom ABI can be provided as the third argument.

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

## Fixed-popcount sets

`set<N, R, Tuint>` enumerates the `N`-bit unsigned values whose popcount is
`R`. `Tuint` defaults to the native register type for `N` equal to 8, 16, 32,
or 64. For every other width it defaults to the sparse
`stdx::uint_indices<N, R>` representation. Supply an exact-width
representation explicitly to opt into a dense bitset:

```cpp
using byte_pairs = jrmwng::popcount_ranges::set<8, 2>;
using sparse_pairs = jrmwng::popcount_ranges::set<8192, 2>;
using dense_pairs =
    jrmwng::popcount_ranges::set<8192, 2, jrmwng::popcount_ranges::stdx::uint<8192>>;
```

See [benchmarks/README.md](benchmarks/README.md) for benchmark targets and usage.
