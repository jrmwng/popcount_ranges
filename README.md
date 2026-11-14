# popcount_ranges

**Enumerate, rank and sample fixed-size combinations.**

`popcount_ranges` is a header-only C++23 library. Its
`jrmwng::popcount_ranges::set<N, R>` represents every choice of exactly `R`
positions from `N` as a bitmask. Iterate the choices or use a rank to jump
directly to one. The sizes are compile-time parameters.

## Mark 6 lottery draw

Choose six main numbers from 49, then draw the special number uniformly from
the 43 remaining numbers. `set<49, 6>` selects a uniformly random main draw by
rank; the special number is selected by rank among the numbers not in that
draw.

```cpp
#include <popcount_ranges/set.hpp>
#include <popcount_ranges/pdep.hpp>

#include <cstddef>
#include <iostream>
#include <random>

int main()
{
    namespace pc = jrmwng::popcount_ranges;

    // 1. Setup the draw engine
    std::mt19937_64 draw_engine(std::random_device{}());
    auto const draw_balls = [&draw_engine](auto && ball_set)
    {
        return ball_set[std::uniform_int_distribution<std::size_t>(0u, ball_set.size() - 1u)(draw_engine)];
    };

    // 2. Lucky Draw (49 numbered balls)
    auto const  normal_6_49_balls = draw_balls(pc::set<49  , 6>{}); // 6 balls out of 49 balls
    auto const special_1_43_balls = draw_balls(pc::set<49-6, 1>{}); // 1 ball out of 43=(49-6) balls

    auto const special_1_49_balls = pc::stdx::pdep(special_1_43_balls, ~normal_6_49_balls); // 1 ball out of 49 balls

    // 3. Output Results
    auto &os = std::cout;

    os << "Main:";
    for (auto const main_pos : pc::stdx::set_bit_positions(normal_6_49_balls))
        os << ' ' << (main_pos + 1);
    os << std::endl;
    
    os << "Special:";
    for (auto const spec_pos : pc::stdx::set_bit_positions(special_1_49_balls))
        os << ' ' << (spec_pos + 1);
    os << std::endl;

    return 0;
}
```

There are `C(49, 6) * 43 = 601,304,088` role-specific outcomes. The special
number is a separate role, so using `set<49, 7>` alone would lose which of the
seven selected numbers is special. `stdx::pdep` deposits the one-bit rank into
the available-number mask, using BMI2 when supported and a portable fallback
otherwise. The same fixed-cardinality selection and rank-based sampling pattern
applies to feature selection, fixed-size test configurations and subset-sum
searches.

## Benchmarks

These examples compare complete application paths on an AMD Ryzen 9 5950X
running Windows 11. Times are **median nanoseconds per operation** across
Release runs; speedup is reference time divided by set time, using unrounded
medians. MSVC and Clang used MSVC STL; GCC used libstdc++.
Compiler and library effects are combined, and results vary by machine and
workload.

### Uniform sampling

Compare `std::sample` with drawing a uniform rank and enumerating the selected
positions. The `8192, 2` set uses the default sparse representation.
Each time is the median of three runs.

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
materializes a list of matches. Both use the same AVX2 byte-lane weighted
sum for `uint8_t` weights and produce the same counts and checksums. These
historical `uint8_t` timings are medians of five runs.
`conditional` and `masked` describe the source-level match update; compilers
may choose different machine instructions.

| Compiler | N, R | Match update | Brute force ns | `set` ns | Speedup |
|---|---:|---|---:|---:|---:|
| MSVC 19.51 | 20, 2 | conditional | 454,122 | 337 | 1,347.54x |
| Clang 23.1 | 20, 2 | conditional | 226,372 | 343 | 659.98x |
| GCC 16.2 | 20, 2 | conditional | 231,631 | 378 | 612.78x |
| MSVC 19.51 | 20, 10 | conditional | 620,637 | 307,933 | 2.02x |
| MSVC 19.51 | 20, 10 | masked | 634,152 | 372,367 | 1.70x |
| Clang 23.1 | 20, 10 | conditional | 406,563 | 360,080 | 1.13x |
| Clang 23.1 | 20, 10 | masked | 434,532 | 330,316 | 1.32x |
| GCC 16.2 | 20, 10 | conditional | 407,838 | 367,586 | 1.11x |
| GCC 16.2 | 20, 10 | masked | 624,650 | 388,179 | 1.61x |

A speedup below 1 means the set path was slower. These selected cases are
illustrative, not a general performance guarantee; see the
[benchmark guide](benchmarks/README.md) for workload details, methodology,
and results for signed and unsigned 8-, 16- and 32-bit weights and `float`.

## Use with CMake

Install the headers and CMake package to a prefix:

```text
cmake -S . -B build-package -DBUILD_TESTING=OFF -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=OFF
cmake --install build-package --prefix <install-prefix>
```

In your application's `CMakeLists.txt`, use the installed package:

```cmake
find_package(popcount_ranges CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE jrmwng::popcount_ranges)
```

If needed, configure your application with
`-DCMAKE_PREFIX_PATH=<install-prefix>` so CMake can find the package.
The library is header-only; `target_link_libraries` adds no library binary.
It propagates the include directory, C++23 requirement, AVX2 compiler option
and SIMD backend definition to your application.

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

## License

This project is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE).
