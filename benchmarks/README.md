# `lower_bound` benchmark

Configure a Release build with benchmarks enabled:

```text
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DPOPCOUNT_RANGES_BUILD_BENCHMARKS=ON
cmake --build build-release --target lower_bound_benchmark
build-release/lower_bound_benchmark.exe --quick
```

The executable prints CSV rows containing element type, sort direction, array size, implementation, nanoseconds per query, and a checksum. The checksum is an optimization guard and should agree across implementations for each case. Use Release or RelWithDebInfo for timing; Debug results are useful only for correctness and diagnostics.