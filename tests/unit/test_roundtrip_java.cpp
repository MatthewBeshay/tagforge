// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using tagforge::Format;

namespace {

std::vector<std::byte> read_file(const std::filesystem::path &p)
{
	std::ifstream f(p, std::ios::binary | std::ios::ate);
	REQUIRE(f.is_open());
	const std::streamsize n = f.tellg();
	f.seekg(0);
	std::vector<std::byte> out(static_cast<std::size_t>(n));
	f.read(reinterpret_cast<char *>(out.data()), n);
	return out;
}

constexpr const char *kFixtureDir = TAGFORGE_FIXTURE_DIR;

} // namespace

TEST_CASE("bigtest_raw.nbt round-trips byte-exactly through decode+encode", "[roundtrip][java][bigtest]")
{
	const auto original = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");

	auto decoded = tagforge::decode(original, Format::JavaNamedRoot);
	REQUIRE(decoded.has_value());

	auto encoded = tagforge::encode(*decoded, Format::JavaNamedRoot);
	REQUIRE(encoded.has_value());

	REQUIRE(encoded->size() == original.size());
	REQUIRE(*encoded == original);
}

TEST_CASE("encode_anonymous EndTag emits a single 0x00", "[roundtrip][java]")
{
	tagforge::Value v{.v = tagforge::EndTag{}};
	auto enc = tagforge::encode_anonymous(v, Format::JavaAnonymousRoot);
	REQUIRE(enc.has_value());
	REQUIRE(enc->size() == 1);
	REQUIRE(static_cast<std::uint8_t>((*enc)[0]) == 0x00);
}

TEST_CASE("encode named root with EndTag value emits a single 0x00", "[roundtrip][java]")
{
	tagforge::NamedValue nv{.name = "ignored", .value = tagforge::Value{.v = tagforge::EndTag{}}};
	auto enc = tagforge::encode(nv, Format::JavaNamedRoot);
	REQUIRE(enc.has_value());
	REQUIRE(enc->size() == 1);
	REQUIRE(static_cast<std::uint8_t>((*enc)[0]) == 0x00);
}

TEST_CASE("encode -> decode round-trip preserves values for primitives", "[roundtrip][java]")
{
	tagforge::Compound c;
	tagforge::upsert(c, "by", tagforge::Value{.v = static_cast<std::int8_t>(-7)});
	tagforge::upsert(c, "sh", tagforge::Value{.v = static_cast<std::int16_t>(31'415)});
	tagforge::upsert(c, "in", tagforge::Value{.v = static_cast<std::int32_t>(0x7FFF'FFFF)});
	tagforge::upsert(c, "lo", tagforge::Value{.v = static_cast<std::int64_t>(0x0123'4567'89AB'CDEFLL)});
	tagforge::upsert(c, "fl", tagforge::Value{.v = 1.5f});
	tagforge::upsert(c, "do", tagforge::Value{.v = 3.14159265358979});
	tagforge::upsert(c, "st", tagforge::Value{.v = std::string{"hello"}});

	tagforge::NamedValue nv{.name = "root", .value = tagforge::Value{.v = std::move(c)}};

	auto enc = tagforge::encode(nv, Format::JavaNamedRoot);
	REQUIRE(enc.has_value());

	auto dec = tagforge::decode(*enc, Format::JavaNamedRoot);
	REQUIRE(dec.has_value());

	REQUIRE(dec->name == "root");
	const auto &c2 = std::get<tagforge::Compound>(dec->value.v);
	REQUIRE(*tagforge::get_byte(c2, "by") == -7);
	REQUIRE(*tagforge::get_short(c2, "sh") == 31'415);
	REQUIRE(*tagforge::get_int(c2, "in") == 0x7FFF'FFFF);
	REQUIRE(*tagforge::get_long(c2, "lo") == 0x0123'4567'89AB'CDEFLL);
	REQUIRE(*tagforge::get_float(c2, "fl") == 1.5f);
	REQUIRE(*tagforge::get_double(c2, "do") == 3.14159265358979);
	REQUIRE(*tagforge::get_string(c2, "st") == "hello");
}

TEST_CASE("encode rejects oversized strings", "[roundtrip][java][errors]")
{
	std::string huge(70'000, 'a');
	tagforge::NamedValue nv{.name = std::move(huge), .value = tagforge::Value{.v = static_cast<std::int32_t>(1)}};
	auto enc = tagforge::encode(nv, Format::JavaNamedRoot);
	REQUIRE_FALSE(enc.has_value());
	REQUIRE(enc.error().code == tagforge::ErrorCode::LengthOverflow);
}
