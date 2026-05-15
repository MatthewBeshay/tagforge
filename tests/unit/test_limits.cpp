// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/decode.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/skip.hpp"

#include <cstddef>
#include <vector>

using tagforge::ErrorCode;
using tagforge::Format;

namespace {

// Build a hand-crafted Java named-root payload that nests N empty compounds:
//   tag(0x0A) name_len(0) ... [nested compound] 0x00 ...
// Trailing 0x00 bytes close each compound.
std::vector<std::byte> deeply_nested_java(std::size_t depth)
{
	std::vector<std::byte> out;
	out.reserve(depth * 3 + depth);
	for (std::size_t i = 0; i < depth; ++i) {
		out.push_back(std::byte{0x0A}); // TAG_Compound
		out.push_back(std::byte{0x00}); // name_len BE high
		out.push_back(std::byte{0x00}); // name_len BE low
	}
	for (std::size_t i = 0; i < depth; ++i) {
		out.push_back(std::byte{0x00}); // TAG_End
	}
	return out;
}

} // namespace

TEST_CASE("decode rejects pathologically nested compounds", "[decode][limits]")
{
	const auto bytes = deeply_nested_java(2000);
	auto r = tagforge::decode(bytes, Format::JavaNamedRoot);
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error().code == ErrorCode::LimitExceeded);
}

TEST_CASE("decode accepts moderately deep compounds", "[decode][limits]")
{
	// 64 is well below the cap and well above any real-world NBT.
	const auto bytes = deeply_nested_java(64);
	auto r = tagforge::decode(bytes, Format::JavaNamedRoot);
	REQUIRE(r.has_value());
}

TEST_CASE("skip rejects pathologically nested compounds", "[skip][limits]")
{
	const auto bytes = deeply_nested_java(2000);
	auto r = tagforge::skip(bytes, Format::JavaNamedRoot);
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error().code == ErrorCode::LimitExceeded);
}
