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

### Other applications

The same range can enumerate fixed-cardinality candidates for subset-sum or
knapsack searches, feature selection, or exhaustive testing of configurations
with exactly `R` enabled options. Rank intervals can divide that work into
disjoint shards: start at `choices.begin() + first_rank` and stop at
`choices.begin() + last_rank`. The application tests compare subset-sum
results with independent bitmask enumeration, including sharded traversal and
a sparse wide set. The subset-sum benchmark compares this approach with
brute-force bitmask filtering; it is not a comparison with optimized solvers.

## Sampling benchmark results

The results below are historical measurements of the earlier, larger
benchmark matrix. The current default build uses a smaller representative
matrix; the exhaustive sampling experiment remains an explicit target. See
the [benchmark guide](benchmarks/README.md) for current workloads and commands.

The sampling benchmark compares selecting `R` positions from `N` using
`std::sample` with drawing a uniform rank in `popcount_ranges::set<N, R>` and
enumerating its set-bit positions. The rank lookup uses the SIMD-assisted
`lower_bound`, so `POPCOUNT_RANGES_SIMD` selects the backend measured by the
`popcount_ranges` rows. `std_sample` is an independent reference path.

These earlier full Release runs used an AMD Ryzen 9 5950X, AVX2, and 100,000
repetitions per workload. They compare complete sampling strategies, not the
lower-bound alternatives measured below. The table includes the 370 nonempty
workloads (`R > 0`). Absolute `std::sample` times are shown alongside the
geometric-mean speedup so a larger multiplier is not mistaken for a shorter
runtime.

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

### Lower-bound implementation comparison

I compared both custom variants: SIMD coarse/fine search **without descent**,
and the same search **with `lower_bound_descent`** selecting a coarse chunk
first. Both were compared with `std::lower_bound`; descent thresholds were 1,
2, 4, 8, 12, 16, 20, 24, 28, and 32. These Release runs used AVX2 on an AMD
Ryzen 9 5950X with the C++26 `<simd>` backend disabled. All variants produced matching
checksums. The table shows the best fixed
strategy from the higher-repetition finalist runs: each value is the geometric
mean of per-workload median end-to-end nanoseconds across the 370 nonempty
workloads (`R > 0`).

| Compiler and standard library | Best fixed strategy | ns/sample | Custom without descent | ns/sample |
|---|---|---:|---|---:|
| Clang 23.1.0, MSVC STL | custom + descent, threshold 32 | 134.21 | custom lower_bound | 137.59 |
| GCC 15.2.0, libstdc++ (WSL2) | `std::lower_bound` | 158.53 | custom lower_bound | 177.68 |
| MSVC 19.51.36257, MSVC STL | `std::lower_bound` | 183.72 | custom lower_bound | 280.77 |

Threshold 32 was the best fixed with-descent threshold among the finalists for
Clang and GCC. Descent thresholds 28 and 32 were close for Clang; GCC's standard
library implementation was faster overall than either custom variant. MSVC
strongly favored `std::lower_bound`.

The per-workload winner files compare custom-without-descent,
custom-with-descent, and `std::lower_bound`. They use `type,N,R` as keys because
dense and sparse representations can share the same `(N,R)`. They exclude `R=0`, where unranking
does not search. The all-threshold screening used five `--quick` runs (1,000
repetitions each); finalists were remeasured in three `--medium` runs (10,000
repetitions each). Per-pair margins were often small for Clang (median 1.08%)
and GCC (2.46%), so those dispatch choices are candidates to validate with
`--full` on the deployment machine, not universal rules. The results are saved
under `build/sampling_benchmark_results/`; see the
[benchmark guide](benchmarks/README.md) for targets and reproduction commands.
The all-threshold screening is summarized in
`sampling-benchmark-winners.csv`; the higher-repetition shortlist is in
`sampling-benchmark-medium-winners.csv`.

### Isolated lower-bound search

The measurements in this section are archived results from the earlier
experimental search harness. The current `lower_bound_search_benchmark` is a
smaller custom-versus-`std::lower_bound` comparison; the former descent,
hybrid, branch-hint, and cache-state modes are no longer part of that
executable. See [benchmarks/README.md](benchmarks/README.md) for current
benchmark usage.

The end-to-end sampling benchmark is not a reliable way to infer per-call search
latency: rank generation, unranking, and population traversal all contribute to
each sample. I added a separate lower-bound microbenchmark using uniform search
positions over evenly spaced arrays, with five-batch medians repeated in three
Release runs. It compares custom search without descent, that search with
descent, and `std::lower_bound`. Calls use equivalent no-inline wrappers to
prevent the compiler from vectorizing across the query batch. The "Custom
coarse+SIMD fine" column is the version **without** descent; the last column is
the best measured version **with** descent. On this Clang 23.1.0 / MSVC STL
build:

| Element width | N | Custom coarse+SIMD fine | `std::lower_bound` | Best descent threshold |
|---:|---:|---:|---:|---:|
| 8-bit | 8 | **3.14 ns** | 3.83 ns | 1: 3.13 ns |
| 16-bit | 16 | 4.96 ns | **4.39 ns** | 8: 4.80 ns |
| 32-bit | 32 | **4.23 ns** | 4.61 ns | 4: 4.18 ns |
| 32-bit | 512 | 14.61 ns | **8.13 ns** | 4: 13.32 ns |
| 32-bit | 8192 | 91.89 ns | **12.61 ns** | 8: 20.29 ns |
| 64-bit | 16 | 3.99 ns | 4.24 ns | 16: 3.81 ns |
| 64-bit | 8192 | 176.71 ns | **16.50 ns** | 16: 24.93 ns |

This supports both of your expectations, with an important qualification:
custom coarse+SIMD search is competitive and can win on short ranges (for
example, 32-bit `N=32`), while descent clearly improves on the linear coarse
scan for long ranges (at 32-bit `N=8192`, about 4.5x faster than that baseline).
The relative performance of descent versus `std::lower_bound` changes with
range size, so I extended the 64-bit experiment.

#### Large 64-bit ranges

These results use six independent query seeds. Each timing is the median of
five batches of 32 repetitions; query counts are reduced for larger arrays to
bound the cost of the sequential baseline. `custom_with_descent/std` is the
ratio of the best of the tested descent thresholds to `std::lower_bound`; a
ratio below 1 favors descent.

| N | `std::lower_bound` | Best custom with descent | Descent / std |
|---:|---:|---:|---:|
| 8,192 | 17.32 ns | threshold 4: 26.05 ns | 1.50x |
| 16,384 | 20.57 ns | threshold 4: 23.73 ns | 1.15x |
| 32,768 | 25.21 ns | threshold 4: 20.30 ns | 0.81x |
| 65,536 | 33.93 ns | threshold 8: 21.99 ns | 0.65x |
| 131,072 | 40.94 ns | threshold 8: 22.90 ns | 0.56x |
| 262,144 | 54.63 ns | threshold 8: 24.30 ns | 0.44x |

This shows the descent-to-standard ratio falling as `N` grows: the best tested
descent changes from 15% slower at 16,384 to 19% faster at 32,768, and is more
than 2x faster at 262,144. Meanwhile, the custom-without-descent scan reaches
about 145x the `std::lower_bound` time at `N=262,144`.

The earlier hybrid results are withdrawn: their `first`/`last` head loop did
not compile like `std::lower_bound`, so they did not establish hybrid
performance with a standard-library-style binary head.

#### Standard-library-style binary head and custom tail

The corrected binary head follows the MSVC STL's iterator-plus-count
`_Lower_bound_unchecked` loop and stops once the tail reaches the threshold.
Optimized assembly confirms conditional moves update its bounds, matching the
relevant code-generation strategy of `std::lower_bound`. An eight-seed,
interleaved comparison of the old index-based and corrected pointer/count
versions, with the same queries for both, reproduces the size-dependent
reversal. It also measures each binary head alone, returning its lower-bound
index without searching a tail:

| N | Threshold | `std::lower_bound` | Old head | New head | Old hybrid | New hybrid |
|---:|---:|---:|---:|---:|---:|---:|
| 32,768 | 16 | 24.98 ns | 39.90 ns | 17.10 ns | 41.72 ns | 29.56 ns |
| 65,536 | 64 | 32.65 ns | 10.45 ns | 18.33 ns | 12.61 ns | 36.47 ns |

These are medians across eight per-seed medians; all full-search variants
returned the same checksums. Head-only checksums intentionally differ because
they capture the provisional binary-phase index rather than the final answer.
This head-only measurement shows that the old loop itself accounts for most
of the surprising 65,536-element win (and the 32,768-element loss), rather
than the custom tail creating that reversal. The tail still matters: the
corrected hybrid loses to `std::lower_bound` at both sizes, and its
complete-search timing is not predicted by the isolated head timing.

The two loops differ in source shape. The old version tracks integer `first`
and `last` indices, recomputes `last - first`, and updates one bound in a
branch. The new version tracks a pointer `first` and remaining `count`, tests
`count > threshold`, and updates `first`/`count` using the STL-style
iterator-plus-count loop. They are semantically equivalent for these searches,
but compile differently: the old loop is branchy and the corrected loop uses
conditional moves in the inspected Clang build. The measured reversal is real
for this compiler, standard library, CPU, and benchmark setup; assembly alone
does not explain its cause. Do not use the old 12.6 ns result as a production
dispatch threshold until the workload and code-generation effects are
understood.

Next, test the causes rather than tuning from this one workload:

1. Collect per-process hardware counters (cycles, instructions, branch
   instructions/misses, and cache misses) for both heads at both sizes, pinned
   to one core if available.
2. Repeat with query orders held constant, shuffled, sorted, and repeated, to
   separate branch-prediction effects from memory-access effects.
3. Vary data alignment and array placement, and run several interleaved
   repetitions with reversed old/new order to detect code-layout or run-order
   sensitivity.
4. Inspect optimized assembly for the exact binary and benchmark build, then
   re-run across compiler/STL combinations before considering a dispatch rule.

The latest archived measurements are in
`build/sampling_benchmark_results/binary-hybrid-old-vs-new-heads.csv`.

The corrected head itself can match the standard-library search. For `N=16,384`,
threshold 1, the four-seed component medians are 19.42 ns for the binary head,
2.90 ns for the custom tail on its precomputed window, 1.39 ns for
`std::lower_bound` on that same tail, 28.76 ns for the complete hybrid, and
21.29 ns for full-range `std::lower_bound`. A control using the corrected head
and a standard-library tail takes 21.32 ns. Thus the head is not the cause of
the full hybrid losing at this `N`; adding the custom tail makes the composed
search slower. Component timings are diagnostic and need not sum exactly to
complete-search timings.

The corrected hybrid follows MSVC STL's `_Lower_bound_unchecked` source loop
shape: pointer iterator plus remaining count, with the loop stopping when the
tail threshold is reached. Clang's optimized assembly uses conditional moves
for the data-dependent bound update, as in `std::lower_bound`. The previous
integer-index variants did not get the same code generation. This is
compiler/STL-specific; inspect optimized output on each target.

The eight-seed dense sweep is in
`build/sampling_benchmark_results/binary-hybrid-threshold-dense-consistent.csv`;
the paired legacy-loop diagnostic is in
`build/sampling_benchmark_results/binary-hybrid-legacy-head.csv`.

### Earlier custom SIMD versus C++26 `<simd>` comparison

In an earlier GCC 16.0.1 run with threshold 28, the C++26 backend had a 1.29x
geometric-mean speedup over the custom backend (0.775x runtime) and was faster
in 84.9% of nonempty workloads. Checksums matched across all 776 output rows.

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

## Requirements

- A C++23 compiler.
- An x86 CPU with AVX2 support when using the CMake target. The target propagates
  the AVX2 compiler option to consumers.
- CMake 3.20 or newer.
- When using the standard SIMD backend, a C++26 compiler and standard library
  providing `<simd>`.

By default, `lower_bound` uses the custom search for 1-byte and 2-byte types,
with a one-cache-line sequential pitch (`64 / sizeof(T)`) and a four-cache-line
binary-search threshold (`256 / sizeof(T)`). The default binary-search
algorithm is branched for 1-byte types and branchless for 2-byte types. Types
of 4 bytes or larger use `std::lower_bound`. The custom path uses the existing
`data_par` SIMD implementation, which has no scalar backend. Including
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

Run both Release and Debug configurations: the main set test runs its
exhaustive cases in Release and its exception checks when `_DEBUG` is defined.
The application and helper tests run in both configurations.

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
