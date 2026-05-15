// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/value.hpp"

#include <cstdint>

using tagforge::Compound;
using tagforge::EndTag;
using tagforge::ErrorCode;
using tagforge::List;
using tagforge::TagId;
using tagforge::Value;

namespace {

Value v_byte(std::int8_t x)
{
	return Value{.v = x};
}
Value v_short(std::int16_t x)
{
	return Value{.v = x};
}
Value v_int(std::int32_t x)
{
	return Value{.v = x};
}
Value v_long(std::int64_t x)
{
	return Value{.v = x};
}
Value v_float(float x)
{
	return Value{.v = x};
}
Value v_double(double x)
{
	return Value{.v = x};
}
Value v_string(std::string s)
{
	return Value{.v = std::move(s)};
}
Value v_list(List l)
{
	return Value{.v = std::move(l)};
}
Value v_compound(Compound c)
{
	return Value{.v = std::move(c)};
}

} // namespace

TEST_CASE("Value::kind returns the variant index as TagId", "[value]")
{
	REQUIRE(v_byte(1).kind() == TagId::Byte);
	REQUIRE(v_short(1).kind() == TagId::Short);
	REQUIRE(v_int(1).kind() == TagId::Int);
	REQUIRE(v_long(1).kind() == TagId::Long);
	REQUIRE(v_float(1.0f).kind() == TagId::Float);
	REQUIRE(v_double(1.0).kind() == TagId::Double);
	REQUIRE(v_string("x").kind() == TagId::String);
	REQUIRE(v_list({}).kind() == TagId::List);
	REQUIRE(v_compound({}).kind() == TagId::Compound);
	REQUIRE(Value{}.kind() == TagId::End);
}

TEST_CASE("Value::list_arm reports the homogeneous arm of a list", "[value]")
{
	List l;
	l.push_back(v_int(1));
	l.push_back(v_int(2));
	Value v{.v = std::move(l)};
	REQUIRE(v.kind() == TagId::List);
	REQUIRE(v.list_arm() == TagId::Int);

	REQUIRE(v_list({}).list_arm() == TagId::End);
	REQUIRE(v_int(1).list_arm() == TagId::End);
}

TEST_CASE("find walks Compound by name in insertion order", "[value]")
{
	Compound c;
	c.emplace_back("a", v_int(1));
	c.emplace_back("b", v_int(2));

	REQUIRE(tagforge::find(c, "a") != nullptr);
	REQUIRE(tagforge::find(c, "b") != nullptr);
	REQUIRE(tagforge::find(c, "missing") == nullptr);
}

TEST_CASE("upsert preserves position on replace", "[value]")
{
	Compound c;
	c.emplace_back("a", v_int(1));
	c.emplace_back("b", v_int(2));
	c.emplace_back("c", v_int(3));

	tagforge::upsert(c, "b", v_int(99));

	REQUIRE(c.size() == 3);
	REQUIRE(c[0].first == "a");
	REQUIRE(c[1].first == "b");
	REQUIRE(std::get<std::int32_t>(c[1].second.v) == 99);
	REQUIRE(c[2].first == "c");
}

TEST_CASE("upsert appends on insert", "[value]")
{
	Compound c;
	tagforge::upsert(c, "z", v_int(7));
	REQUIRE(c.size() == 1);
	REQUIRE(c[0].first == "z");
	REQUIRE(std::get<std::int32_t>(c[0].second.v) == 7);
}

TEST_CASE("Typed get_* accessors return the expected arm", "[value]")
{
	Compound c;
	c.emplace_back("by", v_byte(7));
	c.emplace_back("sh", v_short(123));
	c.emplace_back("in", v_int(456));
	c.emplace_back("lo", v_long(789));
	c.emplace_back("fl", v_float(1.5f));
	c.emplace_back("do", v_double(3.25));
	c.emplace_back("st", v_string("hi"));
	c.emplace_back("co", v_compound({}));

	REQUIRE(*tagforge::get_byte(c, "by") == 7);
	REQUIRE(*tagforge::get_short(c, "sh") == 123);
	REQUIRE(*tagforge::get_int(c, "in") == 456);
	REQUIRE(*tagforge::get_long(c, "lo") == 789);
	REQUIRE(*tagforge::get_float(c, "fl") == 1.5f);
	REQUIRE(*tagforge::get_double(c, "do") == 3.25);
	REQUIRE(*tagforge::get_string(c, "st") == "hi");
	REQUIRE((*tagforge::get_compound(c, "co"))->empty());
}

TEST_CASE("get_bool reads non-zero TAG_Byte as true", "[value]")
{
	Compound c;
	c.emplace_back("t", v_byte(1));
	c.emplace_back("f", v_byte(0));
	REQUIRE(*tagforge::get_bool(c, "t") == true);
	REQUIRE(*tagforge::get_bool(c, "f") == false);
}

TEST_CASE("Missing keys return UnknownTagId", "[value]")
{
	Compound c;
	auto r = tagforge::get_int(c, "missing");
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error().code == ErrorCode::UnknownTagId);
}

TEST_CASE("Wrong-arm reads return UnexpectedRootType", "[value]")
{
	Compound c;
	c.emplace_back("a", v_string("not an int"));
	auto r = tagforge::get_int(c, "a");
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error().code == ErrorCode::UnexpectedRootType);
}

TEST_CASE("get_byte_array / get_int_array / get_long_array return spans", "[value]")
{
	Compound c;
	c.emplace_back("ba", Value{.v = std::vector<std::int8_t>{1, 2, 3}});
	c.emplace_back("ia", Value{.v = std::vector<std::int32_t>{10, 20}});
	c.emplace_back("la", Value{.v = std::vector<std::int64_t>{100, 200}});

	auto ba = tagforge::get_byte_array(c, "ba");
	REQUIRE(ba.has_value());
	REQUIRE(ba->size() == 3);
	REQUIRE((*ba)[0] == 1);

	auto ia = tagforge::get_int_array(c, "ia");
	REQUIRE(ia.has_value());
	REQUIRE(ia->size() == 2);
	REQUIRE((*ia)[1] == 20);

	auto la = tagforge::get_long_array(c, "la");
	REQUIRE(la.has_value());
	REQUIRE((*la)[0] == 100);
}
