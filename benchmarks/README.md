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