// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "bench_common.hpp"

#include "tagforge/decode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/skip.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>

static void BM_skip_bigtest(benchmark::State &state)
{
	const auto &bytes = bench::bigtest_raw();
	for (auto _ : state) {
		auto r = tagforge::skip(bytes, tagforge::Format::JavaNamedRoot);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * bytes.size());
}
BENCHMARK(BM_skip_bigtest);

static void BM_decode_bigtest(benchmark::State &state)
{
	const auto &bytes = bench::bigtest_raw();
	for (auto _ : state) {
		auto r = tagforge::decode(bytes, tagforge::Format::JavaNamedRoot);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * bytes.size());
}
BENCHMARK(BM_decode_bigtest);
