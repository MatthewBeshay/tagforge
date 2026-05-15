// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/skip.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using tagforge::Compound;
using tagforge::Format;
using tagforge::NamedValue;
using tagforge::Value;

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

std::vector<std::byte> bytes(std::initializer_list<std::uint8_t> il)
{
	std::vector<std::byte> out;
	out.reserve(il.size());
	for (auto v : il) {
		out.push_back(static_cast<std::byte>(v));
	}
	return out;
}

NamedValue sample_named(std::string name)
{
	Compound c;
	tagforge::upsert(c, "n", Value{.v = static_cast<std::int32_t>(42)});
	return NamedValue{.name = std::move(name), .value = Value{.v = std::move(c)}};
}

constexpr const char *kFixtureDir = TAGFORGE_FIXTURE_DIR;

} // namespace

TEST_CASE("detect_format identifies Java named-root bigtest", "[detect][java]")
{
	const auto data = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto fmt = tagforge::detect_format(data);
	REQUIRE(fmt.has_value());
	REQUIRE(*fmt == Format::JavaNamedRoot);
}

TEST_CASE("detect_format returns JavaAnonymousRoot for the bare-0x00 sentinel", "[detect]")
{
	const auto data = bytes({0x00});
	auto fmt = tagforge::detect_format(data);
	REQUIRE(fmt.has_value());
	REQUIRE(*fmt == Format::JavaAnonymousRoot);
}

TEST_CASE("detect_format identifies Bedrock LE encoder output", "[detect][bedrock]")
{
	auto encoded = tagforge::encode(sample_named("world"), Format::BedrockLittleEndian);
	REQUIRE(encoded.has_value());
	auto fmt = tagforge::detect_format(*encoded);
	REQUIRE(fmt.has_value());
	REQUIRE(*fmt == Format::BedrockLittleEndian);
}

TEST_CASE("detect_format identifies Bedrock VarInt encoder output", "[detect][bedrock]")
{
	auto encoded = tagforge::encode(sample_named("world"), Format::BedrockVarInt);
	REQUIRE(encoded.has_value());
	auto fmt = tagforge::detect_format(*encoded);
	REQUIRE(fmt.has_value());
	REQUIRE(*fmt == Format::BedrockVarInt);
}

TEST_CASE("detect_format rejects truncated and invalid inputs", "[detect][errors]")
{
	REQUIRE_FALSE(tagforge::detect_format({}).has_value());
	REQUIRE_FALSE(tagforge::detect_format(bytes({0x0A})).has_value());       // only 1 byte
	REQUIRE_FALSE(tagforge::detect_format(bytes({0xFE, 0x00})).has_value()); // invalid tag id
}

TEST_CASE("decode_auto and skip_auto succeed without an explicit Format", "[detect][java]")
{
	const auto data = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto skipped = tagforge::skip_auto(data);
	REQUIRE(skipped.has_value());
	REQUIRE(*skipped == data.size());

	auto decoded = tagforge::decode_auto(data);
	REQUIRE(decoded.has_value());
	REQUIRE(decoded->name == "Level");
}
