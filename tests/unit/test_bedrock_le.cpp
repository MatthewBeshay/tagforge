// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/skip.hpp"
#include "tagforge/value.hpp"

#include <cstdint>
#include <string>

using tagforge::Compound;
using tagforge::Format;
using tagforge::NamedValue;
using tagforge::TagId;
using tagforge::Value;

namespace {

NamedValue make_sample()
{
	Compound c;
	tagforge::upsert(c, "name", Value{.v = std::string{"world"}});
	tagforge::upsert(c, "version", Value{.v = static_cast<std::int32_t>(123)});
	tagforge::upsert(c, "long", Value{.v = static_cast<std::int64_t>(0xDEAD'BEEFLL)});
	tagforge::upsert(c, "f", Value{.v = 2.5f});
	return NamedValue{.name = "root", .value = Value{.v = std::move(c)}};
}

} // namespace

TEST_CASE("BedrockLittleEndian round-trips via decode+encode", "[bedrock][le]")
{
	const auto nv = make_sample();
	auto enc = tagforge::encode(nv, Format::BedrockLittleEndian);
	REQUIRE(enc.has_value());

	auto dec = tagforge::decode(*enc, Format::BedrockLittleEndian);
	REQUIRE(dec.has_value());
	REQUIRE(dec->name == "root");

	auto &c = std::get<Compound>(dec->value.v);
	REQUIRE(*tagforge::get_string(c, "name") == "world");
	REQUIRE(*tagforge::get_int(c, "version") == 123);
	REQUIRE(*tagforge::get_long(c, "long") == 0xDEAD'BEEFLL);
	REQUIRE(*tagforge::get_float(c, "f") == 2.5f);

	auto enc2 = tagforge::encode(*dec, Format::BedrockLittleEndian);
	REQUIRE(enc2.has_value());
	REQUIRE(*enc == *enc2);
}

TEST_CASE("BedrockLittleEndian skip consumes the encoded buffer", "[bedrock][le]")
{
	const auto nv = make_sample();
	auto enc = tagforge::encode(nv, Format::BedrockLittleEndian);
	REQUIRE(enc.has_value());

	auto skip = tagforge::skip(*enc, Format::BedrockLittleEndian);
	REQUIRE(skip.has_value());
	REQUIRE(*skip == enc->size());
}
