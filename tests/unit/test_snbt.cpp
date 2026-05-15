// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/snbt.hpp"
#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"

#include <cstdint>
#include <string>

using tagforge::Compound;
using tagforge::ErrorCode;
using tagforge::List;
using tagforge::SnbtOptions;
using tagforge::TagId;
using tagforge::Value;

TEST_CASE("parse_snbt: integers and type suffixes", "[snbt]")
{
	REQUIRE(tagforge::parse_snbt("1b")->kind() == TagId::Byte);
	REQUIRE(tagforge::parse_snbt("32s")->kind() == TagId::Short);
	REQUIRE(tagforge::parse_snbt("42")->kind() == TagId::Int);
	REQUIRE(tagforge::parse_snbt("9000000000L")->kind() == TagId::Long);
	REQUIRE(std::get<std::int64_t>(tagforge::parse_snbt("9000000000L")->v) == 9000000000LL);

	// Big ints with no suffix promote to long when out of int range.
	REQUIRE(tagforge::parse_snbt("9000000000")->kind() == TagId::Long);
}

TEST_CASE("parse_snbt: floats and doubles", "[snbt]")
{
	REQUIRE(tagforge::parse_snbt("3.14")->kind() == TagId::Double);
	REQUIRE(tagforge::parse_snbt("1.0f")->kind() == TagId::Float);
	REQUIRE(tagforge::parse_snbt("1.0d")->kind() == TagId::Double);
}

TEST_CASE("parse_snbt: booleans become TAG_Byte 1/0", "[snbt]")
{
	REQUIRE(std::get<std::int8_t>(tagforge::parse_snbt("true")->v) == 1);
	REQUIRE(std::get<std::int8_t>(tagforge::parse_snbt("false")->v) == 0);
}

TEST_CASE("parse_snbt: quoted strings + escapes", "[snbt]")
{
	REQUIRE(std::get<std::string>(tagforge::parse_snbt(R"("hello world")")->v) == "hello world");
	REQUIRE(std::get<std::string>(tagforge::parse_snbt(R"("a\"b")")->v) == "a\"b");
	REQUIRE(std::get<std::string>(tagforge::parse_snbt(R"('single')")->v) == "single");
}

TEST_CASE("parse_snbt: compound with unquoted keys", "[snbt]")
{
	auto v = tagforge::parse_snbt("{a:1,b:2L,name:\"hi\"}");
	REQUIRE(v.has_value());
	REQUIRE(v->kind() == TagId::Compound);
	const auto &c = std::get<Compound>(v->v);
	REQUIRE(c.size() == 3);
	REQUIRE(*tagforge::get_int(c, "a") == 1);
	REQUIRE(*tagforge::get_long(c, "b") == 2);
	REQUIRE(*tagforge::get_string(c, "name") == "hi");
}

TEST_CASE("parse_snbt: lists are homogeneous", "[snbt]")
{
	auto v = tagforge::parse_snbt("[1,2,3]");
	REQUIRE(v.has_value());
	REQUIRE(v->kind() == TagId::List);
	REQUIRE(std::get<List>(v->v).size() == 3);

	auto bad = tagforge::parse_snbt("[1, \"oops\"]");
	REQUIRE_FALSE(bad.has_value());
	REQUIRE(bad.error().code == ErrorCode::MixedListArm);
}

TEST_CASE("parse_snbt: typed arrays", "[snbt]")
{
	auto ba = tagforge::parse_snbt("[B; 1b, 2b, 3b]");
	REQUIRE(ba.has_value());
	REQUIRE(ba->kind() == TagId::ByteArray);
	REQUIRE(std::get<std::vector<std::int8_t>>(ba->v).size() == 3);

	auto ia = tagforge::parse_snbt("[I; -1, 0, 1]");
	REQUIRE(ia.has_value());
	REQUIRE(ia->kind() == TagId::IntArray);
	REQUIRE(std::get<std::vector<std::int32_t>>(ia->v)[0] == -1);

	auto la = tagforge::parse_snbt("[L; 100L, 200L]");
	REQUIRE(la.has_value());
	REQUIRE(la->kind() == TagId::LongArray);
}

TEST_CASE("to_snbt round-trips numeric primitives", "[snbt]")
{
	REQUIRE(tagforge::to_snbt(Value{.v = static_cast<std::int8_t>(7)}) == "7b");
	REQUIRE(tagforge::to_snbt(Value{.v = static_cast<std::int16_t>(7)}) == "7s");
	REQUIRE(tagforge::to_snbt(Value{.v = static_cast<std::int32_t>(7)}) == "7");
	REQUIRE(tagforge::to_snbt(Value{.v = static_cast<std::int64_t>(7)}) == "7L");
}

TEST_CASE("to_snbt then parse round-trips compounds", "[snbt]")
{
	Compound c;
	tagforge::upsert(c, "a", Value{.v = static_cast<std::int32_t>(1)});
	tagforge::upsert(c, "b", Value{.v = std::string{"two"}});
	Value v{.v = std::move(c)};

	auto s = tagforge::to_snbt(v);
	auto parsed = tagforge::parse_snbt(s);
	REQUIRE(parsed.has_value());
	REQUIRE(parsed->kind() == TagId::Compound);
	const auto &c2 = std::get<Compound>(parsed->v);
	REQUIRE(*tagforge::get_int(c2, "a") == 1);
	REQUIRE(*tagforge::get_string(c2, "b") == "two");
}

TEST_CASE("to_snbt pretty-prints when requested", "[snbt]")
{
	Compound c;
	tagforge::upsert(c, "x", Value{.v = static_cast<std::int32_t>(1)});
	tagforge::upsert(c, "y", Value{.v = static_cast<std::int32_t>(2)});
	Value v{.v = std::move(c)};

	SnbtOptions opts;
	opts.pretty = true;
	opts.indent_width = 2;
	const auto s = tagforge::to_snbt(v, opts);
	REQUIRE(s.find('\n') != std::string::npos);
	REQUIRE(s.find("  x: 1") != std::string::npos);
}
