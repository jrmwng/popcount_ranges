# Benchmarks

Configure a Release build with benchmarks enabled:

```text
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=ON
cmake --build build-release --target lower_bound_benchmark popcount_set_benchmark sampling_benchmark
build-release/lower_bound_benchmark.exe --quick
build-release/popcount_set_benchmark.exe --quick
build-release/sampling_benchmark.exe --quick
```

The `lower_bound` executable prints CSV rows containing element type, sort direction, array size, implementation, nanoseconds per query, and a checksum. The checksum is an optimization guard and should agree across implementations for each case.

The `popcount_set` executable prints `type,popcount,direction,distance,implementation,ns_per_operation,checksum`. It compares repeated Gosper-based `++`/`--` with direct `advance()` calls, which use `unrank` for the requested offset. Compare rows with the same type, popcount, direction, and distance; the first distance where `unrank_advance` is faster is the observed break-even point for that case.

For sparse wide sets, `jrmwng::popcount_ranges::std::uint_indices<N, R>` stores
the positions of the `R` set bits instead of an `N`-bit value. It can be used as
the value type for `set`, for example:

```cpp
using sparse_value = jrmwng::popcount_ranges::std::uint_indices<8192, 2>;
using sparse_set = jrmwng::popcount_ranges::set<sparse_value{}>;
```

`std::uint_indices<N, R>` exposes sorted set-bit positions through `indices`.
Use `uint<N>` for dense popcounts, where storing the set-bit positions would
take more space. For high popcounts, `std::uint_complement_indices<N, H>`
stores the `H` unset positions instead; its set-bit-position iterator yields
every other position.

Use `--quick` for a smoke test. Omit it for the wider distance sweep. Use Release or RelWithDebInfo for timing; Debug results are useful only for correctness and diagnostics. Timing results depend on the compiler, CPU, and system load, so capture them locally rather than committing CSV output.

The `sampling` executable compares `std::sample` with sampling a uniformly selected
`popcount_ranges::set<R>` rank and then enumerating its set-bit positions. Each CSV
row reports total sample time, a separately measured raw-`std::mt19937` baseline for
the observed number of engine calls, the difference, and actual engine invocations
per sample. The popcount-ranges path makes one logical uniform-rank request per
sample. That request can require multiple raw engine calls when its range needs more
than 32 random bits (because `std::mt19937` produces 32-bit results) and can require
additional calls for distribution rejection. The standard library's engine-call count
is implementation-dependent, so the benchmark measures it instead of assuming it
equals `N`.

The sampling executable reports the compiled `POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD`
only on `popcount_ranges` rows; the threshold column is blank for `std_sample`, which
is independent of that setting. CMake builds the default `sampling_benchmark` at
threshold 4 and also creates `sampling_benchmark_threshold_<value>` targets for the values in
`POPCOUNT_RANGES_SAMPLING_THRESHOLDS` (default:
`1;2;4;8;12;16;20;24;28;32`; threshold 4 uses the
default executable). To compare the defaults, build and run:

```text
cmake --build build-release --target sampling_benchmark sampling_benchmark_threshold_1 sampling_benchmark_threshold_2 sampling_benchmark_threshold_8 sampling_benchmark_threshold_12 sampling_benchmark_threshold_16 sampling_benchmark_threshold_20 sampling_benchmark_threshold_24 sampling_benchmark_threshold_28 sampling_benchmark_threshold_32
build-release/sampling_benchmark.exe > sampling-threshold-4.csv
build-release/sampling_benchmark_threshold_1.exe > sampling-threshold-1.csv
build-release/sampling_benchmark_threshold_2.exe > sampling-threshold-2.csv
build-release/sampling_benchmark_threshold_8.exe > sampling-threshold-8.csv
build-release/sampling_benchmark_threshold_12.exe > sampling-threshold-12.csv
build-release/sampling_benchmark_threshold_16.exe > sampling-threshold-16.csv
build-release/sampling_benchmark_threshold_20.exe > sampling-threshold-20.csv
build-release/sampling_benchmark_threshold_24.exe > sampling-threshold-24.csv
build-release/sampling_benchmark_threshold_28.exe > sampling-threshold-28.csv
build-release/sampling_benchmark_threshold_32.exe > sampling-threshold-32.csv
```

Set `POPCOUNT_RANGES_SAMPLING_THRESHOLDS` at configure time to build a different
matrix. Use the same build type, machine, and repetition mode for all variants.
The `std_sample` rows are a useful per-run reference, but compare the
`popcount_ranges` rows across thresholds to evaluate the threshold effect.

The sampling cases cover every popcount `R` for each listed width where
`C(N,R)` fits in `uint64_t`. The benchmark includes both `R` and `N-R` where
applicable, since those are distinct sampling workloads despite having the same
number of combinations. For every width above 64 bits, it also reports
`sparse_uint<N>` cases for every feasible sparse popcount and its complement.
Low-popcount cases use `std::uint_indices<N, R>`; high-popcount cases use
`std::uint_complement_indices<N, N-R>` to store the unset positions. Their
`std_sample` rows are the same reference workloads with the same population
size and sample count.

To inspect time spent in the sampling stages for wider-integer cases, run a
threshold-specific sampling executable with `--decompose`. This reports rank
generation, the `nCr_search` steps used by unranking in isolation, bit-position
traversal over a reusable batch of preconstructed values, population loads from
precomputed positions, and end-to-end time for dense and sparse `uint8192`
representations with `R=1..5`, plus dense and sparse complement-index
representations with `R=8190` and `R=8191`. The high-popcount decomposition
uses fewer repetitions to limit its memory use. The isolated stages have
different cache and execution conditions from the fused end-to-end loop, so use
them as estimates rather than summing them as an exact decomposition.