// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/compress.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/region.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

using tagforge::Codec;
using tagforge::Compound;
using tagforge::Format;
using tagforge::NamedValue;
using tagforge::Region;
using tagforge::Value;

namespace {

NamedValue sample()
{
	Compound c;
	tagforge::upsert(c, "n", Value{.v = static_cast<std::int32_t>(42)});
	tagforge::upsert(c, "s", Value{.v = std::string{"hello lz4 world"}});
	return NamedValue{.name = "Level", .value = Value{.v = std::move(c)}};
}

} // namespace

TEST_CASE("LZ4 frame magic is detected", "[compress][lz4]")
{
	// LZ4 Frame magic 0x184D2204 stored little-endian.
	const std::vector<std::byte> magic = {std::byte{0x04}, std::byte{0x22}, std::byte{0x4D}, std::byte{0x18}};
	auto c = tagforge::detect_codec(magic);
	REQUIRE(c.has_value());
	REQUIRE(*c == Codec::Lz4);
}

TEST_CASE("LZ4 compress + decompress round-trip", "[compress][lz4]")
{
	std::vector<std::byte> input(4096);
	for (std::size_t i = 0; i < input.size(); ++i) {
		input[i] = static_cast<std::byte>(i & 0xFF);
	}
	for (int level : {1, 6, 9, 12}) {
		auto enc = tagforge::compress(input, Codec::Lz4, level);
		REQUIRE(enc.has_value());
		auto dec = tagforge::decompress(*enc);
		REQUIRE(dec.has_value());
		REQUIRE(*dec == input);
	}
}

TEST_CASE("LZ4 region chunk round-trips (codec byte 4)", "[compress][lz4][region]")
{
	Region r = Region::create();
	REQUIRE(r.write_chunk(1, 2, sample(), Codec::Lz4, /*ts=*/0x12345678U).has_value());

	auto bytes = r.save_to_bytes();
	REQUIRE(bytes.has_value());

	auto reopened = Region::open_from_bytes(std::move(*bytes));
	REQUIRE(reopened.has_value());
	auto val = reopened->chunk_value(1, 2);
	REQUIRE(val.has_value());
	REQUIRE(val->name == "Level");
	REQUIRE(*tagforge::get_int(std::get<Compound>(val->value.v), "n") == 42);
}
