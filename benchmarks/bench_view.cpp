// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "bench_common.hpp"

#include "tagforge/format.hpp"
#include "tagforge/view.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>

namespace {

std::size_t count_compound_entries(const tagforge::CompoundView &cv)
{
	std::size_t n = 0;
	for (auto it = cv.begin(); it != cv.end(); ++it) {
		++n;
	}
	return n;
}

} // namespace

static void BM_view_decode_only(benchmark::State &state)
{
	const auto &bytes = bench::bigtest_raw();
	for (auto _ : state) {
		auto v = tagforge::View::decode(bytes, tagforge::Format::JavaNamedRoot);
		benchmark::DoNotOptimize(v);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * bytes.size());
}
BENCHMARK(BM_view_decode_only);

static void BM_view_decode_and_iterate(benchmark::State &state)
{
	const auto &bytes = bench::bigtest_raw();
	for (auto _ : state) {
		auto v = tagforge::View::decode(bytes, tagforge::Format::JavaNamedRoot);
		if (auto cv = v->as_compound(); cv.has_value()) {
			auto n = count_compound_entries(*cv);
			benchmark::DoNotOptimize(n);
		}
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * bytes.size());
}
BENCHMARK(BM_view_decode_and_iterate);
