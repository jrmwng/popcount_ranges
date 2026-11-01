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

Use `--traits-sweep` to compare independently enabled branchless and branched
binary search thresholds (8 through 1024) with sequential pitches (8, 16, 32,
and 64 elements) on 64-bit arrays of 4,096, 32,768, and 262,144 elements.
Each configuration uses the same uniform queries for a given size and checks
its checksum against `std::lower_bound`. The traits CSV reports strategy,
threshold, pitch, and median nanoseconds per query. Run several `--seed`
values to reduce sensitivity to query sequence and compare geometric means
across the tested sizes; the strategy measurement order alternates with seed
parity to reduce order bias. The best values are workload- and
machine-dependent.

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

### Tuning lower-bound traits

`lower_bound_tuning` searches for faster compile-time traits against
`std::lower_bound`. Build and run it in Release mode:

```text
cmake --build build-release --target lower_bound_tuning
build-release/lower_bound_tuning.exe --stage all --seed 24301 > tuning.csv
```

The pitch stage tests binary-search-disabled implementations at pitches 1, 2,
4, 8, 16, 32, 64, 128, 256, 512, and 1,024 elements. It chooses the
lowest-latency pitch independently for each primitive type and array length.
The threshold stage uses that pitch to test branchless and branched
binary-search thresholds at powers of two from 1 through 1,048,576, plus the
binary-search-disabled result and `std::lower_bound`. The coarse binary search runs while the
remaining range is larger than the threshold. Thresholds smaller than the
sequential pitch and larger than the array are omitted; a threshold equal to
the array size corresponds to the binary-search-disabled baseline.
The supported types are `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `float`,
and `double`. The tested array sizes are 16, 32, 64, 128, 256, 512, 1,024,
2,048, 4,096, 8,196, 16,384, 32,768, 65,536, and 262,144. Integer arrays
larger than the type's distinct value count contain repeated sorted values.

Use `--stage pitch` to emit only pitch results or `--stage threshold` to rerun
the pitch selection and emit only threshold results. `--type uint8_t|uint16_t|uint32_t|uint64_t|float|double`
selects one primitive type; `--type-bits 8|16|32|64` remains available for
selecting unsigned integer widths. `--size N` restricts a run to any positive
array length. Pass `--pitch N` with `--stage pitch` to test one candidate or with
`--stage threshold` to reuse a pitch selected from a multi-seed sweep.
The benchmark uses up to 16,384 unique, evenly spaced lower-bound positions
per workload, selected from the distinct values in the sorted input; integer
types with fewer representable values use fewer queries. `--repetitions N`
sets the number of measured query batches (overriding the default, including
in quick mode); `--seed N` changes the measurement order, and `--quick` reduces
the unique query set to at most 64 positions for a correctness smoke test.
Each timed batch cycles through the unique query set enough times to execute
at least 16,384 searches. The CSV reports both the number of unique positions
and the number of passes per timed batch.
For the defaults, the binary-search threshold is selected from the original
input range length and type. For an untested range length, the next larger
tested size is used; lengths outside the tested interval use the nearest
endpoint. A `std::bit_width` bucket calculation selects the table entry
without a runtime `std::lower_bound`:

| Input range | `uint8_t` | `uint16_t` | `uint32_t` | `uint64_t` | `float` | `double` |
|---:|---:|---:|---:|---:|---:|---:|
| 16 | 32 | 16 | 16 | 16 | 16 | 16 |
| 32 | 32 | 32 | 32 | 32 | 32 | 32 |
| 64 | 64 | 64 | 64 | 64 | 64 | 64 |
| 128 | 128 | 128 | 128 | 128 | 128 | 128 |
| 256 | 256 | 256 | 256 | 256 | 256 | 128 |
| 512 | 512 | 512 | 256 | 128 | 512 | 256 |
| 1,024 | 1,024 | 512 | 1,024 | 256 | 512 | 256 |
| 2,048 | 1,024 | 1,024 | 1,024 | 512 | 512 | 512 |
| 4,096 | 2,048 | 1,024 | 1,024 | 512 | 1,024 | 512 |
| 8,196 | 4,096 | 4,096 | 1,024 | 1,024 | 1,024 | 1,024 |
| 16,384 | 4,096 | 2,048 | 2,048 | 2,048 | 2,048 | 2,048 |
| 32,768 | 4,096 | 4,096 | 4,096 | 2,048 | 4,096 | 2,048 |
| 65,536 | 8,192 | 4,096 | 4,096 | 2,048 | 2,048 | 2,048 |
| 262,144 | 8,192 | 4,096 | 4,096 | 16,384 | 4,096 | 4,096 |

The sequential pitch is looked up by the selected threshold, not by the input
length, using the same bit-width bucket calculation. The pitch table uses the best pitch measured for the matching type and
threshold-sized workload (or the next larger tested size):

| Threshold | `uint8_t` | `uint16_t` | `uint32_t` | `uint64_t` | `float` | `double` |
|---:|---:|---:|---:|---:|---:|---:|
| 16 | 32 | 16 | 8 | 4 | 8 | 4 |
| 32 | 32 | 16 | 8 | 8 | 8 | 8 |
| 64 | 32 | 16 | 16 | 16 | 16 | 16 |
| 128 | 32 | 32 | 32 | 16 | 32 | 16 |
| 256 | 64 | 64 | 32 | 32 | 32 | 16 |
| 512 | 128 | 64 | 32 | 32 | 64 | 32 |
| 1,024 | 128 | 64 | 64 | 32 | 64 | 32 |
| 2,048 | 128 | 128 | 64 | 64 | 64 | 64 |
| 4,096 | 256 | 128 | 128 | 64 | 128 | 64 |
| 8,196 | 256 | 256 | 128 | 128 | 128 | 128 |
| 16,384 | 512 | 256 | 256 | 256 | 256 | 256 |
| 32,768 | 512 | 512 | 512 | 256 | 512 | 256 |
| 65,536 | 1,024 | 512 | 512 | 256 | 512 | 256 |
| 262,144 | 1,024 | 1,024 | 512 | 1,024 | 512 | 1,024 |

Threshold choices minimize the measured geometric mean of the branchless and
branched strategies per `(type, input size)` using the expanded threshold
sweeps. Where needed to keep the threshold at least as large as its
threshold-selected pitch, the selected threshold was raised to that pitch.
Those sweeps used a pitch selected for the input size, not the
threshold-selected pitch shown above. Thus these are data-driven defaults, but
the exact threshold/pitch pairs have not been benchmarked together and do not
establish that the custom implementation beats `std::lower_bound` in every
case. `BINARY_SEARCH_ALGORITHM` remains `none`; algorithm selection remains
separate.

`--processor N` pins runs to one logical processor (group-local on Windows,
system CPU index on Linux), which is useful when comparing separate runs.
Before each measured batch, the utility streams through a buffer twice the
detected LLC size to evict cached data. Windows and Linux use system cache
information for automatic sizing. On other platforms, provide
`--cache-flush-bytes N` with an eviction buffer larger than that machine's LLC.
The CSV records the detected LLC and eviction-buffer sizes, median nanoseconds
per query, and median elapsed nanoseconds for one query batch (`*_ns_per_sample`).
Eviction itself is outside the timed interval; queries within each batch may
still warm the data. Run multiple seeds and compare geometric means before
selecting constants. Rows with different widths, lengths, or seeds should not
be combined as if they were the same workload.

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
