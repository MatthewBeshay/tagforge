// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/compress.hpp"
#include "tagforge/format.hpp"
#include "tagforge/io.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using tagforge::Codec;
using tagforge::Compound;
using tagforge::Format;
using tagforge::NamedValue;
using tagforge::Value;

namespace {

std::filesystem::path temp_path(std::string_view name)
{
	auto p = std::filesystem::temp_directory_path() / "tagforge_tests";
	std::filesystem::create_directories(p);
	return p / name;
}

NamedValue sample()
{
	Compound c;
	tagforge::upsert(c, "n", Value{.v = static_cast<std::int32_t>(42)});
	tagforge::upsert(c, "s", Value{.v = std::string{"hi"}});
	return NamedValue{.name = "root", .value = Value{.v = std::move(c)}};
}

constexpr const char *kFixtureDir = TAGFORGE_FIXTURE_DIR;

} // namespace

TEST_CASE("write_file + read_file round-trip bytes", "[io]")
{
	const auto path = temp_path("rw.bin");
	std::vector<std::byte> payload(64);
	for (std::size_t i = 0; i < payload.size(); ++i) {
		payload[i] = static_cast<std::byte>(i);
	}

	auto w = tagforge::write_file(path, payload);
	REQUIRE(w.has_value());

	auto r = tagforge::read_file(path);
	REQUIRE(r.has_value());
	REQUIRE(*r == payload);
}

TEST_CASE("write_nbt_file + read_nbt_file round-trip (raw)", "[io]")
{
	const auto path = temp_path("rw_raw.nbt");
	auto w = tagforge::write_nbt_file(path, sample(), Format::JavaNamedRoot, Codec::None);
	REQUIRE(w.has_value());

	auto r = tagforge::read_nbt_file(path);
	REQUIRE(r.has_value());
	REQUIRE(r->name == "root");
	REQUIRE(*tagforge::get_int(std::get<Compound>(r->value.v), "n") == 42);
	REQUIRE(*tagforge::get_string(std::get<Compound>(r->value.v), "s") == "hi");
}

TEST_CASE("write_nbt_file + read_nbt_file round-trip (gzip)", "[io]")
{
	const auto path = temp_path("rw_gzip.nbt");
	auto w = tagforge::write_nbt_file(path, sample(), Format::JavaNamedRoot, Codec::Gzip);
	REQUIRE(w.has_value());

	auto r = tagforge::read_nbt_file(path);
	REQUIRE(r.has_value());
	REQUIRE(r->name == "root");
}

TEST_CASE("read_nbt_file decodes the bundled bigtest_gzip fixture", "[io]")
{
	auto r = tagforge::read_nbt_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_gzip.nbt");
	REQUIRE(r.has_value());
	REQUIRE(r->name == "Level");
	const auto &c = std::get<Compound>(r->value.v);
	REQUIRE(*tagforge::get_byte(c, "byteTest") == static_cast<std::int8_t>(127));
}

TEST_CASE("read_file fails cleanly for missing files", "[io][errors]")
{
	auto r = tagforge::read_file(temp_path("does_not_exist.bin"));
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error().code == tagforge::ErrorCode::Io);
}
