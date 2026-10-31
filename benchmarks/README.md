# Benchmarks

Configure a Release build with benchmarks enabled:

```text
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=ON
cmake --build build-release --target lower_bound_benchmark lower_bound_search_benchmark popcount_set_benchmark sampling_benchmark
build-release/lower_bound_benchmark.exe --quick
build-release/lower_bound_search_benchmark.exe --quick
build-release/popcount_set_benchmark.exe --quick
build-release/sampling_benchmark.exe --quick
```

## Lower-bound benchmarks

`lower_bound_benchmark` compares `jrmwng::popcount_ranges::lower_bound` with
`std::lower_bound` for 8-, 16-, 32-, and 64-bit integers. It covers ascending
and descending data at several range sizes. Its CSV columns are
`type,direction,size,implementation,ns_per_query,checksum`. Each custom and
standard-library pair must produce the same checksum.

`lower_bound_search_benchmark` measures the same two implementations on
uniform insertion positions in evenly spaced 64-bit arrays, for sizes 32
through 262,144. The query count scales down for large arrays to bound runtime.
Each reported time is the median of five batches over precomputed queries;
search calls pass through no-inline wrappers, and each checksum is verified
against `std::lower_bound`. Use `--seed <integer>` to reproduce or vary the
query sequence. Its CSV columns are
`compiler,query_seed,type_bits,N,query_distribution,query_count,implementation,median_ns_per_query,checksum`.

Both lower-bound executables accept `--quick` for a shorter correctness
smoke test; omit it for the longer timing run. The search microbenchmark
isolates lookup cost from sampling and rank generation. It is a controlled
workload, not a substitute for measuring the key distribution of an
application.

## Other benchmarks

The `popcount_set` executable prints
`type,popcount,direction,distance,implementation,ns_per_operation,checksum`.
It compares repeated Gosper-based `++`/`--` with direct `advance()` calls,
which use `unrank` for the requested offset. Compare rows with the same type,
popcount, direction, and distance; the first distance where `unrank_advance`
is faster is the observed break-even point for that case.

For sparse wide sets, `jrmwng::popcount_ranges::stdx::uint_indices<N, R>`
stores the positions of the `R` set bits instead of an `N`-bit value. It can
be used as the value type for `set`, for example:

```cpp
using sparse_value = jrmwng::popcount_ranges::stdx::uint_indices<8192, 2>;
using sparse_set = jrmwng::popcount_ranges::set<8192, 2>;
```

`stdx::uint_indices<N, R>` exposes sorted set-bit positions through `indices`.
Use `uint<N>` for dense popcounts, where storing the set-bit positions would
take more space. For high popcounts, `stdx::uint_complement_indices<N, H>`
stores the `H` unset positions instead; its set-bit-position iterator yields
every other position.

Use `--quick` (1,000 repetitions) for a smoke test, `--medium` (10,000
repetitions) for broader comparisons, or omit the option/choose `--full`
(100,000 repetitions) for the full `popcount_set` run. Use Release or
RelWithDebInfo for timing; Debug results are useful only for correctness and
diagnostics. Timing results depend on the compiler, CPU, and system load, so
capture them locally rather than committing CSV output.

The `sampling` executable compares `std::sample` with sampling a uniformly
selected `popcount_ranges::set<N, R>` rank and then enumerating its set-bit
positions. Each CSV row reports total sample time, a separately measured
raw-`std::mt19937` baseline for the observed number of engine calls, the
difference, and actual engine invocations per sample. The popcount-ranges
path makes one logical uniform-rank request per sample. That request can
require multiple raw engine calls when its range needs more than 32 random
bits (because `std::mt19937` produces 32-bit results) and can require
additional calls for distribution rejection. The standard library's
engine-call count is implementation-dependent, so the benchmark measures it
instead of assuming it equals `N`.

The fixed-popcount path reaches `lower_bound` while unranking the sampled
rank, so it is affected by both `POPCOUNT_RANGES_SIMD` and
`POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD`. The `std_sample` path is independent
of both settings. For a C++26 backend comparison, use GCC 16 with a libstdc++
that provides `<simd>` and keep separate build directories for the two
backends:

```text
cmake -S . -B build-gcc16-custom -G Ninja -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=ON -DPOPCOUNT_RANGES_SIMD=OFF
cmake -S . -B build-gcc16-simd -G Ninja -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=ON -DPOPCOUNT_RANGES_SIMD=ON
cmake --build build-gcc16-custom --target sampling_benchmark sampling_benchmark_threshold_24 sampling_benchmark_threshold_28 sampling_benchmark_threshold_32
cmake --build build-gcc16-simd --target sampling_benchmark sampling_benchmark_threshold_24 sampling_benchmark_threshold_28 sampling_benchmark_threshold_32
build-gcc16-custom/sampling_benchmark_threshold_28 > custom-28.csv
build-gcc16-simd/sampling_benchmark_threshold_28 > simd-28.csv
```

Use the same compiler version, standard library, CPU, build type, and threshold
for each backend. Compare `popcount_ranges` rows by workload and verify their
checksums; compare absolute `std_sample` times as well as speedup ratios when
comparing toolchains, since `std::sample`'s engine-call strategy varies by
standard library. The local results in the root README use GCC 16.0.1 for the
custom-versus-`<simd>` comparison, Clang 23.1 and MSVC 19.51 for
custom-backend comparisons, and an AMD Ryzen 9 5950X. On that machine, the
installed Clang and Visual Studio 2026 standard libraries did not include
`<simd>`.

The `simd_adapter_benchmark` executable measures bitset-to-mask construction,
native comparison-mask copying, reduction of preconstructed comparison masks,
and full-width SIMD loads separately. Build and run it in Release mode with
`POPCOUNT_RANGES_SIMD=OFF` and `ON` to compare the legacy and standard SIMD
backends. Standard SIMD range masks use the library's native mask type by
default; range templates can explicitly select `std::bitset` when desired.
Keep compiler, optimization level, CPU, and run conditions the same when
comparing CSV rows.

The sampling executable reports the implementation on each `popcount_ranges`
row; descent rows also report their compiled
`POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD`. `custom_without_descent` measures
coarse-search/SIMD-fine-search directly. `custom_with_descent` adds
`lower_bound_descent` to pick a coarse chunk before using that same lower-bound
search within the chunk. CMake creates `sampling_benchmark_threshold_<value>`
targets for the with-descent variant, with values in
`POPCOUNT_RANGES_SAMPLING_THRESHOLDS` (default:
`1;2;4;8;12;16;20;24;28;32`). `sampling_benchmark_std_lower_bound` measures
the standard-library `std::lower_bound` path. To compare the variants, build
and run:

```text
cmake --build build-release --target sampling_benchmark sampling_benchmark_std_lower_bound sampling_benchmark_threshold_1 sampling_benchmark_threshold_2 sampling_benchmark_threshold_4 sampling_benchmark_threshold_8 sampling_benchmark_threshold_12 sampling_benchmark_threshold_16 sampling_benchmark_threshold_20 sampling_benchmark_threshold_24 sampling_benchmark_threshold_28 sampling_benchmark_threshold_32
build-release/sampling_benchmark.exe > sampling-threshold-32.csv
build-release/sampling_benchmark_std_lower_bound.exe > sampling-std-lower-bound.csv
build-release/sampling_benchmark_threshold_1.exe > sampling-threshold-1.csv
build-release/sampling_benchmark_threshold_2.exe > sampling-threshold-2.csv
build-release/sampling_benchmark_threshold_4.exe > sampling-threshold-4.csv
build-release/sampling_benchmark_threshold_8.exe > sampling-threshold-8.csv
build-release/sampling_benchmark_threshold_12.exe > sampling-threshold-12.csv
build-release/sampling_benchmark_threshold_16.exe > sampling-threshold-16.csv
build-release/sampling_benchmark_threshold_20.exe > sampling-threshold-20.csv
build-release/sampling_benchmark_threshold_24.exe > sampling-threshold-24.csv
build-release/sampling_benchmark_threshold_28.exe > sampling-threshold-28.csv
build-release/sampling_benchmark_threshold_32.exe > sampling-threshold-descent-32.csv
```
