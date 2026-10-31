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

The `lower_bound` executable prints CSV rows containing element type, sort direction, array size, implementation, nanoseconds per query, and a checksum. The checksum is an optimization guard and should agree across implementations for each case.

The `lower_bound_search` executable isolates the search cost from sampling and
rank generation. It compares your two custom variants—coarse/fine SIMD search
without descent and the same search with `lower_bound_descent` selecting a
coarse chunk first—with `std::lower_bound`. Descent thresholds 1, 4, 8, 16,
and 32 are tested on uniform insertion positions for 8-, 16-, 32-, and 64-bit
elements and array sizes 1 through 8192 (8-bit cases stop at 128), plus
64-bit sizes 16384 through 262144. Large-range query counts scale down with
`N` to keep the sequential-search workload bounded; the query count is in each
CSV row. Use `--seed <integer>` to generate a different reproducible query
sequence, and compare multiple seeds to account for data-dependent branch
behavior. Each reported time is the median of five batches over the same
precomputed queries;
descent index construction is excluded because it is amortized across searches.
Every variant's checksum is checked against `std::lower_bound`. The output
includes compiler, element width, range size, query distribution, query count,
implementation, threshold, time, and checksum, so compare by compiler and
`(type_bits,N)`.
Search calls pass through equivalent no-inline wrappers to prevent the compiler
from vectorizing or folding batches of independent queries differently for the
standard-library path. The workload uses unique, evenly spaced keys and uniform
insertion positions; it is a controlled search microbenchmark, not a replacement
for measuring the exact key distribution in an application.
Use `--quick` for a smoke test; omit it for the longer timing run. This targeted
microbenchmark is a better tool for short-range break-even points and long-range
descent behavior than the end-to-end sampling benchmark. Its
`binary_then_custom` rows test a hybrid: binary search narrows to at most the
reported element-count threshold, then the existing custom SIMD search handles
the remaining range. For 64-bit keys, thresholds 1, 2, 4, 8, 16, 32, 64, and
128 are tested. These thresholds are in elements, not bytes.
Use `--breakdown-only` to run a focused 64-bit comparison for `N=32` through
`N=16,384`. It reports the binary head by itself, custom and standard-library
searches over the same precomputed tail windows, and complete binary-head/tail
searches with either tail implementation. The binary head uses the
iterator-plus-count loop structure from the installed standard-library
implementation, stopping when the requested tail size remains. Component
timings are separate measurements and need not sum exactly to complete-search
timings; generated code can differ by compiler.
Use `--hybrid-only` to benchmark the corrected hybrid plus both baselines for
64-bit ranges from `N=32` through `262,144`, without the other element widths
or descent variants.
Use `--hybrid-pair-only` for a denser tail-threshold sweep at `N=32,768` and
`65,536`: it tests thresholds 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96,
128, and 256. For each threshold it reports old index-based and corrected
pointer/count hybrids, and the head-only timings for thresholds 16, 24, 32,
48, 64, 96, and 128. The head-only variants return the insertion index after
the binary phase and do not search the remaining tail. Use identical seeds
when comparing runs; each seed generates the same queries for both versions.
Use `--branch-only` to compare an unannotated binary head with `[[likely]]`
and `[[unlikely]]` versions at `N=32,768` and `65,536`. It tests repeated
all-false and all-true comparison paths, one repeated path with alternating
directions, and uniform random insertion positions. This identifies highly
predictable and less predictable workloads, but cannot guarantee that every
branch is mispredicted or predicted: that depends on the CPU's predictor and
its history. The annotations guide compiler optimization; they do not control
hardware predictions. Use hardware performance counters to measure actual
branch misses. `--quick` can check output and checksums, but use the full mode
for timing comparisons. On the Clang 23.1.0 / Ryzen 9 5950X run, the four-seed
median unannotated times were about 8.8 ns for constant-true versus 54.7 ns
for uniform-random queries at `N=32,768`, and 9.8 ns versus 63.2 ns at
`N=65,536`. These random-query timings are not a pure misprediction penalty:
random queries also visit less-localized data. The tests do not establish a
100% branch-misprediction case; that requires hardware-counter verification
and would be CPU/predictor-specific. The full results are in
`build/sampling_benchmark_results/binary-head-branch-patterns.csv`.
Use `--cache-state-only` to compare an LLC-sized working set before searches
at `N=4,096`, `8,192`, `16,384`, `32,768`, `65,536`, `131,072`, and
`262,144`. By default, the working set is 64 MiB; override it with
`--llc-mib <size>`. Each mode samples 513 evenly spaced insertion
positions in shuffled order (9 with `--quick`) and prepares the cache state
before every timed search:

* `llc_dirty_working_set`: write one word in each 64-byte line of the working
  set.
* `llc_flushed`: `CLFLUSH` every working-set line and every target-array line,
  then fence.
* `llc_flushed_array_prefetched`: perform the same flush, then issue T0
  prefetches and demand-read one value per target-array cache line.

This working-set sweep dirties or flushes the specified memory region, not
arbitrary unrelated lines owned by other processes. A 64 MiB aggregate region
also spans both L3 instances on a Ryzen 9 5950X; an individual core's local L3
is 32 MiB, so use `--llc-mib 32` to test a single-CCD-sized region. The write
pass does not guarantee every line remains simultaneously dirty in one
particular cache level. Results include per-search TSC cycles and nanoseconds;
the TSC frequency is calibrated against `steady_clock` in the pinned process
using three 250 ms samples, and timer overhead remains included. On Windows
the benchmark pins its thread to the first CPU allowed by the process and
reports that logical CPU; the maximum single observation can still include OS
interruption noise. They are cache-state comparisons, not guaranteed
branch-prediction extremes.
These experiments are expensive because the working set is rewritten or
flushed before every sampled position. The expanded run covers 513 evenly
spaced positions (not every one of the N+1 possible positions), with the same
shuffled positions across methods and thresholds for each N. It compares
`binary_then_custom`, `old_binary_then_custom`, and `std_lower_bound`; both
hybrids use the same thresholds 16, 32, and 64. Search wrappers are no-inline
and return the exact lower-bound index. Cache setup is repeated independently
for every method/threshold. The best/worst per-position measurements in the
CSV can include interruptions.

The 257-position results are in
`build/sampling_benchmark_results/binary-search-n-sweep-257-llc64.csv` and
`build/sampling_benchmark_results/binary-search-n-sweep-257-llc32.csv`.
The 513-position runs are written to
`build/sampling_benchmark_results/binary-search-n-sweep-513-llc64.csv` and
`build/sampling_benchmark_results/binary-search-n-sweep-513-llc32.csv`.
Earlier, less-dense 65-position sweeps remain in
`binary-search-n-sweep-llc64.csv` and `binary-search-n-sweep-llc32.csv`.
The head-only 65-position experiment is separate, in
`binary-head-llc-cache-states.csv` and `binary-head-llc32-cache-states.csv`.

For the 64 MiB working set, the following tables compare best medians
(ns/search) from the 257- and 513-position runs. Hybrid thresholds are
selected independently from 16, 32, and 64 for each run; `std::lower_bound`
has no threshold.

**Dirty working set**

| N | `binary_then_custom` 257 → 513 | `old_binary_then_custom` 257 → 513 | `std::lower_bound` 257 → 513 |
|---:|---:|---:|---:|
| 4,096 | 1,543 (T32) → 1,533 (T64) | 1,132 (T64) → 1,132 (T16) | 1,383 → 1,342 |
| 8,192 | 1,563 (T64) → 1,673 (T32) | 1,222 (T32) → 1,252 (T64) | 1,523 → 1,453 |
| 16,384 | 1,813 (T64) → 1,773 (T32) | 1,383 (T32) → 1,383 (T16) | 1,683 → 1,653 |
| 32,768 | 1,974 (T64) → 1,884 (T64) | 1,483 (T16) → 1,463 (T16) | 1,914 → 1,813 |
| 65,536 | 2,184 (T64) → 2,164 (T64) | 1,553 (T32) → 1,613 (T16) | 2,184 → 2,104 |
| 131,072 | 2,455 (T32) → 2,414 (T64) | 1,643 (T32) → 1,583 (T16) | 2,475 → 2,354 |
| 262,144 | 2,605 (T64) → 2,585 (T64) | 1,623 (T16) → 1,843 (T64) | 2,585 → 2,635 |

**Flushed working set**

| N | `binary_then_custom` 257 → 513 | `old_binary_then_custom` 257 → 513 | `std::lower_bound` 257 → 513 |
|---:|---:|---:|---:|
| 4,096 | 701 (T64) → 721 (T64) | 491 (T64) → 531 (T64) | 771 → 781 |
| 8,192 | 791 (T64) → 822 (T64) | 541 (T64) → 571 (T16) | 862 → 882 |
| 16,384 | 872 (T64) → 922 (T64) | 641 (T16) → 651 (T32) | 962 → 982 |
| 32,768 | 962 (T64) → 992 (T64) | 651 (T32) → 691 (T32) | 1,052 → 1,082 |
| 65,536 | 1,062 (T64) → 1,092 (T64) | 671 (T16) → 751 (T16) | 1,162 → 1,182 |
| 131,072 | 1,162 (T64) → 1,232 (T64) | 681 (T32) → 771 (T32) | 1,252 → 1,312 |
| 262,144 | 1,272 (T64) → 1,312 (T64) | 711 (T32) → 812 (T16) | 1,353 → 1,383 |

**Flushed, then array prefetched/touched**

| N | `binary_then_custom` 257 → 513 | `old_binary_then_custom` 257 → 513 | `std::lower_bound` 257 → 513 |
|---:|---:|---:|---:|
| 4,096 | 30 (T16) → 40 (T16) | 40 (T16) → 60 (T16) | 30 → 30 |
| 8,192 | 40 (T16) → 40 (T16) | 50 (T16) → 60 (T16) | 40 → 50 |
| 16,384 | 40 (T64) → 50 (T16) | 50 (T16) → 60 (T16) | 50 → 60 |
| 32,768 | 50 (T16) → 70 (T16) | 60 (T16) → 70 (T16) | 60 → 80 |
| 65,536 | 80 (T16) → 100 (T16) | 70 (T16) → 90 (T16) | 90 → 110 |
| 131,072 | 130 (T64) → 150 (T32) | 100 (T16) → 110 (T32) | 160 → 170 |
| 262,144 | 180 (T64) → 180 (T32) | 110 (T16) → 120 (T16) | 210 → 210 |

The tables are ordered dirty working set, flushed, and flushed then array
prefetched/touched, respectively. In the dirty and flushed states, the old
hybrid remains fastest among the tested methods at every N in both sweeps.
Increasing position count changes many medians only modestly, but there are
notable shifts (for example, old hybrid at N=262,144 with a 64 MiB flushed
working set moves from 711 to 812 ns). The prefetched state has smaller
absolute times and should not be conflated with the cold-search states. These
are still single runs; position count alone does not provide repeated-run
confidence intervals or identify the cause of the old hybrid's advantage.
For the pinned Clang 23.1.0 / Ryzen 9 5950X run, median search time across
positions was:

| N | Working set | Dirty pass | Flushed | Flushed + array prefetch/touch |
|---:|---:|---:|---:|---:|
| 32,768 | 32 MiB | 831.55 ns | 460.86 ns | 50.09 ns |
| 32,768 | 64 MiB | 1,272.37 ns | 571.06 ns | 50.09 ns |
| 65,536 | 32 MiB | 911.70 ns | 601.12 ns | 70.13 ns |
| 65,536 | 64 MiB | 1,182.20 ns | 571.06 ns | 70.13 ns |

The dirty-pass and flushed timings include the search after preparation, not
the time spent dirtying or flushing the working set. The table therefore does
not measure dirty-line writeback cost directly.

The `popcount_set` executable prints `type,popcount,direction,distance,implementation,ns_per_operation,checksum`. It compares repeated Gosper-based `++`/`--` with direct `advance()` calls, which use `unrank` for the requested offset. Compare rows with the same type, popcount, direction, and distance; the first distance where `unrank_advance` is faster is the observed break-even point for that case.

For sparse wide sets, `jrmwng::popcount_ranges::stdx::uint_indices<N, R>` stores
the positions of the `R` set bits instead of an `N`-bit value. It can be used as
the value type for `set`, for example:

```cpp
using sparse_value = jrmwng::popcount_ranges::stdx::uint_indices<8192, 2>;
using sparse_set = jrmwng::popcount_ranges::set<8192, 2>;
```

`stdx::uint_indices<N, R>` exposes sorted set-bit positions through `indices`.
Use `uint<N>` for dense popcounts, where storing the set-bit positions would
take more space. For high popcounts, `stdx::uint_complement_indices<N, H>`
stores the `H` unset positions instead; its set-bit-position iterator yields
every other position.

Use `--quick` (1,000 repetitions) for a smoke test, `--medium` (10,000 repetitions)
for broader comparisons, or omit the option/choose `--full` (100,000 repetitions)
for the full run. Use Release or RelWithDebInfo for timing; Debug results are
useful only for correctness and diagnostics. Timing results depend on the compiler,
CPU, and system load, so capture them locally rather than committing CSV output.

The `sampling` executable compares `std::sample` with sampling a uniformly selected
`popcount_ranges::set<N, R>` rank and then enumerating its set-bit positions. Each CSV
row reports total sample time, a separately measured raw-`std::mt19937` baseline for
the observed number of engine calls, the difference, and actual engine invocations
per sample. The popcount-ranges path makes one logical uniform-rank request per
sample. That request can require multiple raw engine calls when its range needs more
than 32 random bits (because `std::mt19937` produces 32-bit results) and can require
additional calls for distribution rejection. The standard library's engine-call count
is implementation-dependent, so the benchmark measures it instead of assuming it
equals `N`.

The fixed-popcount path reaches `lower_bound` while unranking the sampled rank,
so it is affected by both `POPCOUNT_RANGES_SIMD` and
`POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD`. The `std_sample` path is independent of
both settings. For a C++26 backend comparison, use GCC 16 with a libstdc++ that
provides `<simd>` and keep separate build directories for the two backends:

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
custom-versus-`<simd>` comparison, Clang 23.1 and MSVC 19.51 for custom-backend
comparisons, and an AMD Ryzen 9 5950X. On that machine, the installed Clang and
Visual Studio 2026 standard libraries did not include `<simd>`.

The `simd_adapter_benchmark` executable measures bitset-to-mask construction,
native comparison-mask copying, reduction of preconstructed comparison masks,
and full-width SIMD loads separately. Build and run it in Release mode with
`POPCOUNT_RANGES_SIMD=OFF` and `ON` to compare the legacy and standard SIMD
backends. Standard SIMD range masks use the library's native mask type by
default; range templates can explicitly select `std::bitset` when desired.
Keep compiler, optimization level, CPU, and run conditions the same when
comparing CSV rows.

The sampling executable reports the implementation on each `popcount_ranges` row;
descent rows also report their compiled `POPCOUNT_RANGES_LOWER_BOUND_THRESHOLD`.
`custom_without_descent` measures coarse-search/SIMD-fine-search directly.
`custom_with_descent` adds `lower_bound_descent` to pick a coarse chunk before
using that same lower-bound search within the chunk. CMake creates
`sampling_benchmark_threshold_<value>` targets for the with-descent variant,
with the values in
`POPCOUNT_RANGES_SAMPLING_THRESHOLDS` (default:
`1;2;4;8;12;16;20;24;28;32`; threshold 32 uses the
threshold-32 descent variant). `sampling_benchmark_std_lower_bound` measures the
standard-library `std::lower_bound` path. To compare the variants, build and run:

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

Set `POPCOUNT_RANGES_SAMPLING_THRESHOLDS` at configure time to build a different
matrix. Use the same build type, machine, and repetition mode for all variants.
The `std_sample` rows are a useful per-run reference, but compare the
`popcount_ranges` rows across thresholds to evaluate the threshold effect.
The measured threshold differences are small and backend-dependent; threshold 28
was near the best of 24, 28, and 32 in the local full Release comparison, rather
than a universal optimum.

Each row includes the compiler identity. Compare records by `compiler,type,N,R`
(dense and sparse representations can share the same `N,R`) and compare the two
custom variants and `std_lower_bound` by `sample_ns_per_sample`; verify checksums
match within each workload. The sampling cases cover every popcount `R` for each listed width where
`C(N,R)` fits in `uint64_t`. The benchmark includes both `R` and `N-R` where
applicable, since those are distinct sampling workloads despite having the same
number of combinations. For every width above 64 bits, it also reports
`sparse_uint<N>` cases for every feasible sparse popcount and its complement.
Low-popcount cases use `stdx::uint_indices<N, R>`; high-popcount cases use
`stdx::uint_complement_indices<N, N-R>` to store the unset positions. Their
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