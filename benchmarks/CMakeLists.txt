# tagforge in-repo self-benchmarks. Track regressions in tagforge's own
# performance as the library evolves. No external NBT libs.
#
# Cross-library comparisons live in the separate `nbt_performance` repo.

find_package(benchmark CONFIG REQUIRED)

add_executable(tagforge_bench
    bench_decode.cpp
    bench_encode.cpp
    bench_view.cpp
    bench_compress.cpp
    bench_snbt.cpp
)

target_link_libraries(tagforge_bench
    PRIVATE
        tagforge::tagforge
        benchmark::benchmark
        benchmark::benchmark_main
)

tagforge_set_compile_options(tagforge_bench)

target_compile_definitions(tagforge_bench PRIVATE
    TAGFORGE_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)
