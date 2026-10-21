# popcount_ranges

**Enumerate, rank, and sample fixed-size combinations with bitmasks.**

`popcount_ranges` is a header-only C++23 library. Its `set<N, R>` represents
every choice of exactly `R` positions from an `N`-position universe as an
`N`-bit value with exactly `R` bits set. Iterate all choices, or jump directly
to a choice by its rank.

It also provides wide-integer representations and SIMD-assisted `lower_bound`;
the optional C++26 backend uses the standard `<simd>` header.

## At a glance

### Enumerate every 3-of-8 combination

Each yielded value is a bitmask. `set_bit_positions` turns it into the selected
positions (zero-based):

```cpp
#include <popcount_ranges/set.hpp>

#include <cstddef>
#include <iostream>

namespace popcount = jrmwng::popcount_ranges;

int main()
{
    using combinations = popcount::set<8, 3>;

    for (auto const& choice : combinations{})
    {
        for (std::size_t position : popcount::stdx::set_bit_positions(choice))
            std::cout << position << ' ';
        std::cout << '\n';
    }
}
```

The range contains `C(8, 3) = 56` choices, in increasing bitmask order.

### Pick one combination uniformly

Choose a uniform rank, then index directly into the combination range:

```cpp
#include <popcount_ranges/set.hpp>

#include <cstddef>
#include <iostream>
#include <random>

namespace popcount = jrmwng::popcount_ranges;

int main()
{
    using combinations = popcount::set<16, 3>;

    combinations choices;
    std::mt19937 engine{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> rank(0, choices.size() - 1);

    auto const selected = choices[rank(engine)];
    for (std::size_t position : popcount::stdx::set_bit_positions(selected))
        std::cout << position << ' ';
    std::cout << '\n';
}
```

This is useful when the result is naturally a fixed-size subset or bitmask.
Unlike `std::sample`, it does not take an arbitrary input range or runtime
sample count; `N` and `R` are compile-time parameters.

## Sampling benchmark results

The sampling benchmark compares selecting `R` positions from `N` using
`std::sample` with drawing a uniform rank in `popcount_ranges::set<N, R>` and
enumerating its set-bit positions. The rank lookup uses the SIMD-assisted
`lower_bound`, so `POPCOUNT_RANGES_SIMD` selects the backend measured by the
`popcount_ranges` rows. `std_sample` is an independent reference path.

These full Release runs used an AMD Ryzen 9 5950X, AVX2, threshold 28, and
100,000 repetitions per workload. The table includes the 370 nonempty workloads
(`R > 0`). Absolute `std::sample` times are shown alongside the geometric-mean
speedup so a larger multiplier is not mistaken for a shorter runtime.

| Compiler and standard library | SIMD backend | `std::sample` mean | median | geometric mean | `popcount_ranges` geometric-mean speedup | Cases faster |
|---|---|---:|---:|---:|---:|---:|
| Clang 23.1, MSVC STL 14.51 | custom | 5,239 ns | 1,159 ns | 1,368 ns | 9.53x | 100% |
| GCC 16.0.1, libstdc++ 16 | custom | 1,741 ns | 356 ns | 549 ns | 3.36x | 95.4% |
| GCC 16.0.1, libstdc++ 16 | C++26 `<simd>` | 1,726 ns | 363 ns | 545 ns | 4.32x | 100% |
| MSVC 19.51, MSVC STL 14.51 | custom | 5,665 ns | 1,253 ns | 1,436 ns | 4.75x | 89.5% |

On this machine, `std::sample`'s absolute mean time with GCC/libstdc++ was about
3.0x lower than with Clang/MSVC STL and 3.3x lower than with MSVC/MSVC STL.
This comparison includes the standard-library implementation: `std::sample`
averaged 494 raw `mt19937` calls per nonempty workload with libstdc++, versus
1,122 with MSVC STL. For `N=8192, R=2`, the measured counts were 2,739 and
8,192 calls respectively. Clang and MSVC used the same MSVC STL on this machine.

### Custom SIMD versus C++26 `<simd>`

With GCC 16.0.1 and threshold 28, the C++26 backend had a 1.29x geometric-mean
speedup over the custom backend (0.775x runtime) and was faster in 84.9% of
nonempty workloads. Checksums matched across all 776 output rows.

| Workload | Custom | C++26 `<simd>` | Speedup |
|---|---:|---:|---:|
| `uint8`, `R=2` | 39.51 ns | 7.25 ns | 5.45x |
| `uint16`, `R=1` | 4.18 ns | 4.22 ns | 0.99x |
| `uint32`, `R=2` | 26.52 ns | 8.67 ns | 3.06x |
| `uint128`, `R=2` | 33.51 ns | 19.08 ns | 1.76x |
| dense `uint8192`, `R=2` | 177.60 ns | 175.23 ns | 1.01x |
| sparse `uint8192`, `R=2` | 42.70 ns | 28.77 ns | 1.48x |

The standard-SIMD build was tested with GCC 16.0.1 and its libstdc++ `<simd>`
header. The installed Clang 23.1 and Visual Studio 2026/MSVC 19.51 standard
libraries did not provide `<simd>`, so C++26-backend results are not available
for those toolchains on this machine.

### Threshold comparison

Thresholds 24, 28, and 32 were also compared in full Release runs with GCC 16.
The geometric-mean `popcount_ranges` time across the same 370 nonempty workloads
was:

| `POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD` | Custom SIMD | C++26 `<simd>` |
|---:|---:|---:|
| 24 | 162.78 ns/sample | 125.41 ns/sample |
| 28 | 163.13 ns/sample | 126.34 ns/sample |
| 32 | 163.67 ns/sample | 124.15 ns/sample |

Threshold 28 is effectively tied with 24 for the custom backend (within 0.3%),
while 32 was about 1.7% faster than 28 for the C++26 backend. These are small,
backend- and workload-dependent differences from single full runs; the results
support 28 as a near-optimal setting, not as a universal exact optimum. See the
[benchmark guide](benchmarks/README.md) for reproduction commands and
measurement details.

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
`R`. Its `operator[]` selects a value directly by rank. `Tuint` defaults to
the smallest native register type that can hold `N`
bits for widths up to 64. Wider sets default to the sparse
`stdx::uint_indices<N, R>` representation. The register-backed set treats bits
above `N` as outside its value domain. Supply an explicit representation to
opt into a dense bitset:

```cpp
using byte_pairs = jrmwng::popcount_ranges::set<8, 2>;
using five_bit_pairs = jrmwng::popcount_ranges::set<5, 2>;
using sparse_pairs = jrmwng::popcount_ranges::set<8192, 2>;
using dense_pairs =
    jrmwng::popcount_ranges::set<8192, 2, jrmwng::popcount_ranges::stdx::uint<8192>>;
```

See [benchmarks/README.md](benchmarks/README.md) for benchmark targets and usage.
