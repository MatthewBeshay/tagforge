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
#include <vector>

using tagforge::Compound;
using tagforge::Format;
using tagforge::List;
using tagforge::NamedValue;
using tagforge::TagId;
using tagforge::Value;

namespace {

NamedValue make_sample()
{
	Compound c;
	tagforge::upsert(c, "msg", Value{.v = std::string{"hello bedrock"}});
	tagforge::upsert(c, "small", Value{.v = static_cast<std::int32_t>(-1)});
	tagforge::upsert(c, "large", Value{.v = static_cast<std::int32_t>(0x7FFF'FFFF)});
	tagforge::upsert(c, "neglong", Value{.v = static_cast<std::int64_t>(-2'000'000'000'000LL)});

	Value list{.v = List{}};
	std::get<List>(list.v).push_back(Value{.v = static_cast<std::int32_t>(1)});
	std::get<List>(list.v).push_back(Value{.v = static_cast<std::int32_t>(-2)});
	std::get<List>(list.v).push_back(Value{.v = static_cast<std::int32_t>(3)});
	tagforge::upsert(c, "ints", std::move(list));

	tagforge::upsert(c, "ia", Value{.v = std::vector<std::int32_t>{-1, 0, 1, 2}});
	tagforge::upsert(c, "la", Value{.v = std::vector<std::int64_t>{-100, 100}});

	return NamedValue{.name = "r", .value = Value{.v = std::move(c)}};
}

} // namespace

TEST_CASE("BedrockVarInt round-trips ZigZag VarInts via decode+encode", "[bedrock][varint]")
{
	const auto nv = make_sample();
	auto enc = tagforge::encode(nv, Format::BedrockVarInt);
	REQUIRE(enc.has_value());

	auto dec = tagforge::decode(*enc, Format::BedrockVarInt);
	REQUIRE(dec.has_value());
	REQUIRE(dec->name == "r");

	const auto &c = std::get<Compound>(dec->value.v);
	REQUIRE(*tagforge::get_string(c, "msg") == "hello bedrock");
	REQUIRE(*tagforge::get_int(c, "small") == -1);
	REQUIRE(*tagforge::get_int(c, "large") == 0x7FFF'FFFF);
	REQUIRE(*tagforge::get_long(c, "neglong") == -2'000'000'000'000LL);

	auto ints = tagforge::get_list(c, "ints");
	REQUIRE(ints.has_value());
	REQUIRE((*ints)->size() == 3);
	REQUIRE(std::get<std::int32_t>((**ints)[1].v) == -2);

	auto ia = tagforge::get_int_array(c, "ia");
	REQUIRE(ia.has_value());
	REQUIRE(ia->size() == 4);
	REQUIRE((*ia)[0] == -1);

	auto enc2 = tagforge::encode(*dec, Format::BedrockVarInt);
	REQUIRE(enc2.has_value());
	REQUIRE(*enc == *enc2);
}

TEST_CASE("BedrockVarInt skip consumes the encoded buffer", "[bedrock][varint]")
{
	const auto nv = make_sample();
	auto enc = tagforge::encode(nv, Format::BedrockVarInt);
	REQUIRE(enc.has_value());

	auto skip = tagforge::skip(*enc, Format::BedrockVarInt);
	REQUIRE(skip.has_value());
	REQUIRE(*skip == enc->size());
}
