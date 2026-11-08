# Usage and integration

## Requirements

The library is header-only and requires C++23. Its CMake target enables AVX2,
so the resulting programs require an x86 CPU with AVX2 support. CMake 3.20 or
newer is required for the build and package configuration.

The optional `POPCOUNT_RANGES_SIMD=ON` CMake option selects the C++26 `<simd>`
backend and requires a compiler and standard library that provide it. With
the default option, the custom SIMD backend is used. When including the
headers without CMake, define `POPCOUNT_RANGES_SIMD` to `1` for the standard
backend or `0` for the custom one, and select a suitable SIMD compiler target.

## Install and consume

```text
cmake -S . -B build -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix <install-prefix>
```

In another CMake project:

```cmake
find_package(popcount_ranges CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE jrmwng::popcount_ranges)
```

The exported target supplies the include directory, language requirement and
AVX2 compiler option. For a build-tree dependency, use `add_subdirectory`
instead. The [consumer example](../examples/consumer) can be configured with
`-DCMAKE_PREFIX_PATH=<install-prefix>` to verify an installation.

## Fixed-popcount sets

`jrmwng::popcount_ranges::set<N, R, Tuint>` enumerates the masks with exactly
`R` bits set among `N` positions, in increasing mask order. `N` and `R` are
compile-time constants, `N` must be positive, and `R` must not exceed `N`.
The combination count must be representable by `uintmax_t`; iterator offsets
and distances must be representable by `ptrdiff_t`.

The default representation is the smallest native unsigned type that can hold
`N` bits for `N <= 64`; for larger `N` it is
`stdx::uint_indices<N, R>`, which stores the selected positions. Dense and
complement-index representations are available explicitly:

```cpp
namespace pc = jrmwng::popcount_ranges;
using native_pairs = pc::set<8, 2>;
using sparse_pairs = pc::set<8192, 2>;
using dense_pairs = pc::set<8192, 2, pc::stdx::uint<8192>>;
using sparse_near_full =
    pc::set<8192, 8190, pc::stdx::uint_complement_indices<8192, 2>>;
```

`set::size()` returns the number of values in the range and `set::operator[]`
looks up a value by its **full-set rank**. Iterate selected positions with
`stdx::set_bit_positions(value)`. The default set covers all ranks, so
`choices[0]` is the first value and `choices[choices.size() - 1]` is the last.
For `[first, last)` bounded sets constructed from values, indexing still uses
the full-set rank, not an offset from `first`. Iterator arithmetic can divide
the full set into disjoint rank intervals; iterator differences and offsets
must be in range.

## Supporting helpers

`nCr_*`, `uint`, `lower_bound` and the SIMD types support the set's
implementation. The default `lower_bound` traits use a sequential pitch of
`64 / sizeof(T)` and a binary-search threshold of `256 / sizeof(T)`. One-byte
types use branched binary search, two-byte types use branchless binary search,
and larger types dispatch to `std::lower_bound`.

`data_par::simd_range_traits`, `simd_long_range` and `simd_short_range` accept
an optional mask type and ABI. The default mask is the backend's native mask;
`data_par::simd_bitset_mask<T>` can be supplied explicitly.

`stdx::pdep(value, mask)` deposits the low bits of an unsigned value into the
set-bit positions of a mask. On x86 it uses BMI2 when available and otherwise
uses a portable implementation; it does not add a BMI2 CPU requirement.
