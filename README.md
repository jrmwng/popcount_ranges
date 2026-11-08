# popcount_ranges

**Enumerate, rank and sample fixed-size combinations.**

`popcount_ranges` is a header-only C++23 library. Its
`jrmwng::popcount_ranges::set<N, R>` represents every choice of exactly `R`
positions from `N` as a bitmask. Iterate the choices or use a rank to jump
directly to one. The sizes are compile-time parameters.

## Hello, combinations

Select every two-person team from four people:

```cpp
#include <popcount_ranges/set.hpp>

#include <array>
#include <iostream>
#include <string_view>

int main()
{
    namespace pc = jrmwng::popcount_ranges;
    constexpr std::array<std::string_view, 4> people{"Ada", "Bo", "Cleo", "Dan"};

    for (auto team : pc::set<4, 2>{})
    {
        for (auto position : pc::stdx::set_bit_positions(team))
            std::cout << people[position] << ' ';
        std::cout << '\n';
    }
}
```

The output is:

```text
Ada Bo
Ada Cleo
Bo Cleo
Ada Dan
Bo Dan
Cleo Dan
```

The six teams are produced in increasing bitmask order. The same technique
applies to feature selection, fixed-size test configurations and
fixed-cardinality subset-sum searches. For uniform sampling, draw a uniform
integer in `[0, choices.size() - 1]` and use `choices[rank]`; unlike
`std::sample`, the population size and sample size are compile-time constants.

## Benchmarks

These examples compare complete application paths on an AMD Ryzen 9 5950X
running Windows 11. Times are **median nanoseconds per operation** across
three Release runs; speedup is reference time divided by set time, using
unrounded medians. MSVC and Clang used MSVC STL; GCC used libstdc++.
Compiler and library effects are combined, and results vary by machine and
workload.

### Uniform sampling

Compare `std::sample` with drawing a uniform rank and enumerating the selected
positions. The `8192, 2` set uses the default sparse representation.

| Compiler | N, R | `std::sample` ns | `set` ns | Speedup |
|---|---:|---:|---:|---:|
| MSVC 19.51 | 64, 2 | 285.2 | 52.1 | 5.47x |
| Clang 23.1 | 64, 2 | 283.4 | 9.7 | 29.14x |
| GCC 16.2 | 64, 2 | 83.7 | 10.5 | 8.00x |
| MSVC 19.51 | 8192, 2 | 34,102.0 | 50.9 | 669.39x |
| Clang 23.1 | 8192, 2 | 33,807.7 | 21.4 | 1,580.17x |
| GCC 16.2 | 8192, 2 | 7,713.3 | 45.7 | 168.94x |
| MSVC 19.51 | 16, 8 | 129.3 | 247.3 | 0.52x |
| Clang 23.1 | 16, 8 | 128.1 | 69.3 | 1.85x |
| GCC 16.2 | 16, 8 | 83.0 | 80.9 | 1.03x |

The sample timing includes random number generation and population loads.
The `set` path sums selected values directly; `std::sample` writes a
preallocated output vector and then sums it. The methods use different random
sequences, so their checksums need not match.

### Fixed-cardinality subset sum

Count subsets of `R` weights summing to a target and accumulate their masks.
The reference filters **every bitmask** by popcount; it is a simple
brute-force baseline, not an optimized subset-sum solver. Neither path
materializes a list of matches. Both produce the same counts and checksums.

| Compiler | N, R | Brute force ns | `set` ns | Speedup |
|---|---:|---:|---:|---:|
| MSVC 19.51 | 20, 2 | 453,153 | 449 | 1,009.25x |
| Clang 23.1 | 20, 2 | 448,556 | 448 | 1,001.24x |
| GCC 16.2 | 20, 2 | 340,035 | 455 | 747.33x |
| MSVC 19.51 | 20, 10 | 4,711,514 | 1,145,095 | 4.11x |
| Clang 23.1 | 20, 10 | 559,104 | 1,065,108 | 0.52x |
| GCC 16.2 | 20, 10 | 655,883 | 994,361 | 0.66x |

A speedup below 1 means the set path was slower. These selected cases are
illustrative, not a general performance guarantee; see the
[benchmark guide](benchmarks/README.md) for workload details and methodology.

## Build and test

Requires CMake 3.20+, a C++23 compiler and an x86 CPU with AVX2. For a
single-configuration generator such as Ninja:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For the Visual Studio 2026 multi-configuration generator:

```text
cmake -S . -B build-vs -G "Visual Studio 18 2026"
cmake --build build-vs --config Release
ctest --test-dir build-vs -C Release --output-on-failure
```

Set `-DPOPCOUNT_RANGES_BUILD_BENCHMARKS=OFF` to skip benchmark executables.
Run both Release and Debug tests: the main set test runs exhaustive cases in
Release and exception checks when `_DEBUG` is defined. Debug configurations
that do not define `_DEBUG` run the exhaustive cases instead.

See the [usage guide](docs/usage.md) for installation, CMake integration,
representations and SIMD options; see the [benchmark guide](benchmarks/README.md)
for workloads, methodology and reproduction commands.
