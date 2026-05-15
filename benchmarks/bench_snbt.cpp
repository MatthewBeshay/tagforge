// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "bench_common.hpp"

#include "tagforge/snbt.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::string_view kSampleSnbt = R"({
        DataVersion: 3953,
        Items: [
                {id:"minecraft:diamond_sword", Count:1b, tag:{display:{Name:"Excalibur"}}},
                {id:"minecraft:bread",         Count:64b},
                {id:"minecraft:cobblestone",   Count:32b}
        ],
        Pos:    [16.5d, 70.0d, -32.5d],
        Motion: [0.0d, -0.0784d, 0.0d],
        Tags:   ["chunk_loader", "spawn_protected"],
        Nested: {a:{b:{c:{d:{e:1, f:"deep"}}}}}
})";

const tagforge::Value &sample_tree()
{
	static const auto tree = [] {
		auto v = tagforge::parse_snbt(kSampleSnbt);
		if (!v) {
			throw std::runtime_error{"snbt setup parse failed"};
		}
		return *v;
	}();
	return tree;
}

} // namespace

static void BM_parse_snbt(benchmark::State &state)
{
	for (auto _ : state) {
		auto r = tagforge::parse_snbt(kSampleSnbt);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * kSampleSnbt.size());
}
BENCHMARK(BM_parse_snbt);

static void BM_to_snbt_compact(benchmark::State &state)
{
	const auto &tree = sample_tree();
	for (auto _ : state) {
		auto r = tagforge::to_snbt(tree);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * kSampleSnbt.size());
}
BENCHMARK(BM_to_snbt_compact);

static void BM_to_snbt_pretty(benchmark::State &state)
{
	const auto &tree = sample_tree();
	tagforge::SnbtOptions pretty{.pretty = true, .indent_width = 2};
	for (auto _ : state) {
		auto r = tagforge::to_snbt(tree, pretty);
		benchmark::DoNotOptimize(r);
	}
	state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * kSampleSnbt.size());
}
BENCHMARK(BM_to_snbt_pretty);
