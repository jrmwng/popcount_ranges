# Benchmarks

Configure a Release build with benchmarks enabled:

```text
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=ON
cmake --build build-release --target lower_bound_benchmark popcount_set_benchmark
build-release/lower_bound_benchmark.exe --quick
build-release/popcount_set_benchmark.exe --quick
```

The `lower_bound` executable prints CSV rows containing element type, sort direction, array size, implementation, nanoseconds per query, and a checksum. The checksum is an optimization guard and should agree across implementations for each case.

The `popcount_set` executable prints `type,popcount,direction,distance,implementation,ns_per_operation,checksum`. It compares repeated Gosper-based `++`/`--` with direct `advance()` calls, which use `unrank` for the requested offset. Compare rows with the same type, popcount, direction, and distance; the first distance where `unrank_advance` is faster is the observed break-even point for that case.

Use `--quick` for a smoke test. Omit it for the wider distance sweep. Use Release or RelWithDebInfo for timing; Debug results are useful only for correctness and diagnostics. Timing results depend on the compiler, CPU, and system load, so capture them locally rather than committing CSV output.