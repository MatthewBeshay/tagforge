// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/compress.hpp"
#include "tagforge/decode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using tagforge::Codec;
using tagforge::ErrorCode;
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

TEST_CASE("detect_codec identifies gzip magic", "[compress]")
{
	const auto gz = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_gzip.nbt");
	auto c = tagforge::detect_codec(gz);
	REQUIRE(c.has_value());
	REQUIRE(*c == Codec::Gzip);
}

TEST_CASE("detect_codec identifies zlib magic", "[compress]")
{
	const auto zl = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_zlib.nbt");
	auto c = tagforge::detect_codec(zl);
	REQUIRE(c.has_value());
	REQUIRE(*c == Codec::Zlib);
}

TEST_CASE("detect_codec returns nullopt for raw bytes", "[compress]")
{
	const auto raw = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto c = tagforge::detect_codec(raw);
	REQUIRE_FALSE(c.has_value());
}

TEST_CASE("decompress(gzip) recovers the raw bigtest bytes", "[compress]")
{
	const auto gz = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_gzip.nbt");
	const auto raw = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto out = tagforge::decompress(gz);
	REQUIRE(out.has_value());
	REQUIRE(*out == raw);
}

TEST_CASE("decompress(zlib) recovers the raw bigtest bytes", "[compress]")
{
	const auto zl = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_zlib.nbt");
	const auto raw = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto out = tagforge::decompress(zl);
	REQUIRE(out.has_value());
	REQUIRE(*out == raw);
}

TEST_CASE("decompress(gzip) -> decode produces the same tree as the raw fixture", "[compress]")
{
	const auto gz = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_gzip.nbt");
	auto raw = tagforge::decompress(gz);
	REQUIRE(raw.has_value());
	auto tree_from_gz = tagforge::decode(*raw, Format::JavaNamedRoot);
	REQUIRE(tree_from_gz.has_value());

	const auto raw_file = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto tree_from_raw = tagforge::decode(raw_file, Format::JavaNamedRoot);
	REQUIRE(tree_from_raw.has_value());

	REQUIRE(*tree_from_gz == *tree_from_raw);
}

TEST_CASE("compress + decompress round-trip", "[compress]")
{
	const auto raw = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");

	for (auto codec : {Codec::Gzip, Codec::Zlib}) {
		for (int level : {1, 6, 9, 12}) {
			auto enc = tagforge::compress(raw, codec, level);
			REQUIRE(enc.has_value());
			auto dec = tagforge::decompress(*enc);
			REQUIRE(dec.has_value());
			REQUIRE(*dec == raw);
		}
	}
}

TEST_CASE("decompress rejects payloads exceeding max_output", "[compress][errors]")
{
	const auto zl = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_zlib.nbt");
	tagforge::DecompressOptions opts;
	opts.max_output = 16; // far too small for bigtest
	auto out = tagforge::decompress(zl, opts);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::LimitExceeded);
}
