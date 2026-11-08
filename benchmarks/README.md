# Benchmarks

The default benchmarks focus on `popcount_ranges::set` and its applications.
Configure an optimized build on a machine with AVX2:

```text
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=ON
cmake --build build-release
ctest --test-dir build-release --output-on-failure
build-release/set_operations_benchmark.exe --quick
build-release/popcount_set_benchmark.exe --quick
build-release/sampling_benchmark.exe --quick
build-release/subset_sum_benchmark.exe --quick
build-release/lower_bound_benchmark.exe --quick
```

Omit `--quick` for timing runs. Times depend on the CPU, compiler, standard
library and system load. `--quick` is for correctness and smoke testing, not
performance conclusions. Output is CSV on stdout; diagnostics go to stderr.

## Reproducing the README measurements

The README measurements were collected on November 8, 2026 on an AMD Ryzen 9
5950X running Windows 11 Pro (build 26300). All builds used Ninja, CMake
Release settings, AVX2 and the custom SIMD backend
(`POPCOUNT_RANGES_SIMD=OFF`):

* MSVC 19.51.36257 (Visual Studio 2026), MSVC STL 14.51.36231:
  `/O2 /Ob2 /DNDEBUG`.
* Clang 23.1.0, MSVC STL 14.51.36231: `-O3 -DNDEBUG`.
* MSYS2 UCRT64 GCC 16.2.0, libstdc++ 16.2.0: `-O3 -DNDEBUG`.

The raw, unaggregated sampling runs are in
[sampling-2026-11-08.csv](results/sampling-2026-11-08.csv); the five
historical `uint8_t` SIMD subset-sum runs are in
[subset-sum-simd-2026-11-08.csv](results/subset-sum-simd-2026-11-08.csv).
The expanded three-run type comparison is in
[subset-sum-types-2026-11-08.csv](results/subset-sum-types-2026-11-08.csv).
The older [scalar subset-sum runs](results/subset-sum-2026-11-08.csv) are
retained for historical comparison. The sampling and scalar subset-sum runs
were collected at commit `873840e`; the subsequent correction to the
experimental standard-search target does not change the default path.
`toolchain` identifies the compiler and `run` identifies the process-level
repetition.

Configure separate Release directories with the desired compiler on `PATH`
and `-DCMAKE_CXX_COMPILER=cl`, `clang++` or `g++`, respectively. On Windows,
use a Visual Studio Developer Command Prompt for MSVC and Clang, and an MSYS2
UCRT64 environment for GCC. The current subset-sum benchmark covers additional
weight types and differs from the earlier `uint8_t` implementation used for the
root README's historical measurements. To collect the current comparison:

```text
cmake --build build-release --target sampling_benchmark subset_sum_benchmark
build-release/sampling_benchmark.exe --full > sampling-1.csv
build-release/sampling_benchmark.exe --full > sampling-2.csv
build-release/sampling_benchmark.exe --full > sampling-3.csv
build-release/subset_sum_benchmark.exe > subset-sum-1.csv
build-release/subset_sum_benchmark.exe > subset-sum-2.csv
build-release/subset_sum_benchmark.exe > subset-sum-3.csv
```

Each full sampling run uses 100,000 samples per workload; each subset-sum run
uses 100 searches per workload, cycling through four targets. Runs were
sequential, without processor affinity; compiler order was rotated across
repetitions. The root README reports medians of three sampling runs or five
historical SIMD subset-sum runs. The type comparison below uses three runs.
Speedups are the ratio of the two reported median times before rounding, not
the median of individual speedups.
These are illustrative local results, not statistically controlled or portable
performance guarantees. Sampling measures RNG, selection and population
loads together. The standard library affects the random-distribution and
`std::sample` implementations, so compiler and library effects are combined.

## Set operations

`set_operations_benchmark` measures forward and backward traversal, sequential
rank lookup and random rank lookup. It includes native, sparse, dense wide and
complement-index representations. The fixed window is checked element by
element against indexed values and reverse traversal before timing; the three
corresponding timed checksums must agree. Random ranks are precomputed from a
fixed seed. Times include set-bit-position traversal to consume each value.
Columns are `representation,N,R,operation,values,repetitions,ns_per_value,checksum`.

`popcount_set_benchmark` compares repeated increments or decrements with
`advance()` for different distances and native widths. Columns are
`type,reduce_count,direction,distance,implementation,ns_per_operation,checksum`.
Compare checksums for matching type, count, direction and distance.
The benchmark fails if the paired checksums differ.
The normal run uses 10,000 repetitions and `--quick` uses 1,000.

## Applications

`sampling_benchmark` compares `std::sample` with selecting a uniform rank and
enumerating the corresponding set-bit positions. Its default matrix covers
native 8/16/32/64-bit and dense and sparse 128/256/8192-bit representations,
including empty, sparse, near-full and full selections; dense types also include
the largest representable middle popcount. It is intentionally smaller than
the historical exhaustive sweep. Use `--quick`, `--medium` or `--full` for
1,000, 10,000 or 100,000 repetitions, respectively (`--full` is the default).
CSV rows include elapsed time, the separately measured raw `mt19937` baseline,
engine calls per sample and a checksum. The two sampling methods do not
produce the same random samples, so their checksums need not match. Raw
engine-call counts depend on the implementation of the distribution. The
set path sums population values directly as positions are enumerated;
`std::sample` writes a preallocated result vector, which is then summed.

`subset_sum_benchmark` counts fixed-cardinality subsets whose weights sum
to a target, accumulating their masks without materializing a list. Set
traversal and brute-force popcount filtering both use the same AVX2 sum for
each type. Unsigned 8-bit weights use 32 byte lanes and `_mm256_sad_epu8`;
the other integer types use eight 32-bit lanes per chunk and widen to 64-bit
lanes before accumulation. Float uses eight float lanes per chunk.
Integer sums are exact in `int64_t` or `uint64_t`. The benchmark covers
`uint8_t`, `uint16_t`, `uint32_t`, `int8_t`, `int16_t`, `int32_t` and `float`.

The `small` workload uses identical positive integer values and targets for
every type. The `large` integer workload includes each type's minimum and
maximum and deterministic values spanning its bit patterns; the four targets
are sums of different contiguous groups of `R` weights. The `large` float
workload uses signed integer coefficients from -15 to 15 times 2^119. Every
sum of at most 32 of these values is finite and exactly representable, so the
scalar and SIMD reduction orders produce the same result. This is a
large-magnitude workload, not arbitrary floating-point values: general floats
may overflow or round differently and require a separately defined matching
policy. All variants are checked against an independent scalar reference
before timing.

For `N=20, R=10`, the following table shows median microseconds per search
for `set` / brute force, using the `conditional` match update. These runs
used the same machine and compiler settings described above. The large
workloads have different values and targets for each type; timings include
the complete enumeration and match update. The generic SIMD implementation
is not necessarily the fastest possible implementation for each type.

| Weight type | Range | MSVC set / brute | Clang set / brute | GCC set / brute |
|---|---|---:|---:|---:|
| uint8 | small | 314.7 / 612.7 | 366.0 / 567.9 | 367.6 / 417.8 |
| uint8 | large | 310.0 / 613.9 | 365.7 / 562.3 | 366.4 / 409.6 |
| uint16 | small | 1784.1 / 2075.8 | 460.2 / 711.7 | 679.3 / 831.3 |
| uint16 | large | 1775.9 / 2067.4 | 461.2 / 708.9 | 677.9 / 826.2 |
| uint32 | small | 1785.6 / 1927.0 | 627.4 / 842.0 | 678.7 / 817.0 |
| uint32 | large | 1759.1 / 1902.3 | 628.6 / 838.7 | 682.9 / 814.5 |
| int8 | small | 1807.4 / 2002.1 | 454.4 / 620.2 | 678.8 / 830.8 |
| int8 | large | 1815.8 / 2011.5 | 455.9 / 616.5 | 681.5 / 830.9 |
| int16 | small | 1815.1 / 2002.4 | 460.4 / 727.6 | 677.9 / 830.1 |
| int16 | large | 1812.8 / 2005.7 | 474.1 / 720.2 | 685.0 / 827.5 |
| int32 | small | 1767.0 / 1962.7 | 628.9 / 849.7 | 684.9 / 822.3 |
| int32 | large | 1763.9 / 1960.0 | 647.0 / 845.7 | 692.5 / 844.1 |
| float | small | 1698.3 / 1932.9 | 441.9 / 599.0 | 490.6 / 689.4 |
| float | large | 1728.2 / 1944.3 | 442.1 / 616.1 | 502.8 / 685.1 |

Each method has a `conditional` source-level match update and a `masked`
arithmetic update. These names do not guarantee a branch or its absence in
machine code: compilers may transform either version, or other parts of the
loop. Inspect the generated binary before drawing conclusions about branch
prediction. Columns are
`type,range,N,R,implementation,match_handling,repetitions,ns_per_search,matches,mask_checksum`.
The comparison is against brute-force mask filtering, not an optimized
subset-sum algorithm. The historical scalar data used a different sum
implementation and CSV shape.

## Supporting helpers

`lower_bound_benchmark` compares the helper with `std::lower_bound` on
ascending and descending unsigned integer arrays of several widths and sizes;
paired checksums must match. Columns are
`type,direction,size,implementation,ns_per_query,checksum`.
The default traits use a sequential pitch of `64 / sizeof(T)` and a binary
search threshold of `256 / sizeof(T)`; types of four bytes or more dispatch
to `std::lower_bound`.

## Experimental targets

These are excluded from the default build but remain available by name:

* `lower_bound_search_benchmark`: isolated search and `--traits-sweep`
  experiments; supports `--quick` and `--seed <integer>`.
* `lower_bound_tuning`: large type, pitch and threshold sweeps. For example,
  use `--stage default --quick`; build with limited parallelism because its
  template instantiations consume substantial memory.
* `simd_adapter_benchmark`: mask, reduction and load microbenchmarks.
* `sampling_benchmark_experimental`: the former exhaustive sampling matrix,
  including `--decompose` for component timing. This target can take
  considerably longer to compile and run.
* `sampling_benchmark_threshold_<value>` and
  `sampling_benchmark_std_lower_bound`: lower-bound strategy comparisons
  using the smaller default sampling matrix. Threshold targets are generated
  from `POPCOUNT_RANGES_SAMPLING_THRESHOLDS`, which defaults to
  `1;2;4;8;12;16;20;24;28;32`.

The standard-search target uses `std::lower_bound` for binomial table row
searches with `R >= 2`; the `R = 0` and `R = 1` rows use direct formulas.
The `custom_without_descent` label denotes the default dispatcher, which
itself uses `std::lower_bound` for elements of four bytes or more. Versions
before this correction labeled the standard-search target as standard search
but did not change the search path; discard those comparison results.

For example:

```text
cmake --build build-release --target lower_bound_tuning --parallel 3
cmake --build build-release --target sampling_benchmark_threshold_32 sampling_benchmark_std_lower_bound
build-release/sampling_benchmark_threshold_32.exe --quick
build-release/sampling_benchmark_std_lower_bound.exe --quick
```

Use the same compiler, build type, backend and workloads when comparing
variants. Set `POPCOUNT_RANGES_SIMD=ON` in a separate build to test a C++26
`<simd>` implementation where the compiler and standard library provide it.
The current default sampling matrix is intentionally smaller than the
experimental exhaustive matrix; results from the two are not directly
comparable as overall summaries.
