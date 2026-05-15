// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "bench_common.hpp"

#include "tagforge/compress.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>

static void BM_compress_zlib_balanced(benchmark::State &state)
{
	const auto &raw = bench::bigtest_raw();
	for (auto _ : state) {
		auto r = tagforge::compress(raw, tagforge::Codec::Zlib);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * raw.size());
}
BENCHMARK(BM_compress_zlib_balanced);

static void BM_compress_gzip_balanced(benchmark::State &state)
{
	const auto &raw = bench::bigtest_raw();
	for (auto _ : state) {
		auto r = tagforge::compress(raw, tagforge::Codec::Gzip);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * raw.size());
}
BENCHMARK(BM_compress_gzip_balanced);

static void BM_compress_lz4_balanced(benchmark::State &state)
{
	const auto &raw = bench::bigtest_raw();
	for (auto _ : state) {
		auto r = tagforge::compress(raw, tagforge::Codec::Lz4);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * raw.size());
}
BENCHMARK(BM_compress_lz4_balanced);

static void BM_decompress_gzip_autodetect(benchmark::State &state)
{
	const auto &gz = bench::bigtest_gzip();
	const auto raw_size = bench::bigtest_raw().size();
	for (auto _ : state) {
		auto r = tagforge::decompress(gz);
		benchmark::DoNotOptimize(r);
	}
	// Per-byte throughput is reported relative to the *uncompressed* size,
	// so the number is comparable to the compress benchmarks above.
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * raw_size);
}
BENCHMARK(BM_decompress_gzip_autodetect);
