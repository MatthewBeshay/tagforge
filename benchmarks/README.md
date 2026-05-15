# tagforge in-repo benchmarks

Self-benchmarks for tracking tagforge's own performance across changes.
Built on [Google Benchmark](https://github.com/google/benchmark).

## Build & run

```pwsh
cmake -S . -B build/bench `
    -G "Ninja Multi-Config" `
    -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake `
    -DVCPKG_MANIFEST_FEATURES=benchmarks `
    -DTAGFORGE_BUILD_BENCHMARKS=ON `
    -DTAGFORGE_BUILD_TESTING=OFF `
    -DTAGFORGE_BUILD_CLI=OFF
cmake --build build/bench --config Release
.\build\bench\benchmarks\Release\tagforge_bench.exe
```

## What's measured

| TU | Workload | Why |
|---|---|---|
| `bench_decode.cpp`   | `bigtest_raw.nbt` decode + skip | Cursor / decode-tree throughput. |
| `bench_encode.cpp`   | encode the decoded bigtest tree | Encoder throughput; complement to decode. |
| `bench_view.cpp`     | `View::decode` + iterate top-level compound | Lazy-navigation overhead. |
| `bench_compress.cpp` | gzip / zlib / lz4 round-trip on bigtest | libdeflate + lz4 codec performance. |
| `bench_snbt.cpp`     | parse + emit (compact/pretty) of a representative SNBT string | SNBT lexer/parser/writer. |

Each calls `state.SetBytesProcessed(iterations * input.size())`, so Google Benchmark reports throughput per byte processed.
