// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "bench_common.hpp"

#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/format.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

const tagforge::NamedValue &bigtest_tree()
{
	static const auto tree = [] {
		const auto &bytes = bench::bigtest_raw();
		auto nv = tagforge::decode(bytes, tagforge::Format::JavaNamedRoot);
		if (!nv) {
			throw std::runtime_error{"decode failed during encode-bench setup"};
		}
		return std::move(*nv);
	}();
	return tree;
}

} // namespace

static void BM_encode_bigtest(benchmark::State &state)
{
	const auto &tree = bigtest_tree();
	const auto reference_size = bench::bigtest_raw().size();
	for (auto _ : state) {
		auto r = tagforge::encode(tree, tagforge::Format::JavaNamedRoot);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * reference_size);
}
BENCHMARK(BM_encode_bigtest);
