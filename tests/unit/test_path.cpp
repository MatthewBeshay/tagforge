// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/path.hpp"
#include "tagforge/value.hpp"

#include <cstdint>
#include <string>
#include <vector>

using tagforge::Compound;
using tagforge::ErrorCode;
using tagforge::List;
using tagforge::Path;
using tagforge::PathSegment;
using tagforge::Value;

namespace {

Value sample()
{
	Compound display;
	tagforge::upsert(display, "Name", Value{.v = std::string{"Excalibur"}});

	Compound tag;
	tagforge::upsert(tag, "display", Value{.v = std::move(display)});

	Compound item0;
	tagforge::upsert(item0, "id", Value{.v = std::string{"minecraft:diamond_sword"}});
	tagforge::upsert(item0, "tag", Value{.v = std::move(tag)});

	Compound item1;
	tagforge::upsert(item1, "id", Value{.v = std::string{"minecraft:bread"}});

	List items;
	items.push_back(Value{.v = std::move(item0)});
	items.push_back(Value{.v = std::move(item1)});

	Compound c;
	tagforge::upsert(c, "Items", Value{.v = std::move(items)});
	tagforge::upsert(c, "weird key", Value{.v = static_cast<std::int32_t>(7)});

	return Value{.v = std::move(c)};
}

} // namespace

TEST_CASE("parse_path: simple compound chain", "[path]")
{
	auto p = tagforge::parse_path("a.b.c");
	REQUIRE(p.has_value());
	REQUIRE(p->size() == 3);
	REQUIRE(p->segments()[0].kind == PathSegment::Kind::Key);
	REQUIRE(p->segments()[0].key == "a");
	REQUIRE(p->segments()[2].key == "c");
}

TEST_CASE("parse_path: list index", "[path]")
{
	auto p = tagforge::parse_path("Items[0]");
	REQUIRE(p.has_value());
	REQUIRE(p->size() == 2);
	REQUIRE(p->segments()[0].key == "Items");
	REQUIRE(p->segments()[1].kind == PathSegment::Kind::Index);
	REQUIRE(p->segments()[1].index == 0);
}

TEST_CASE("parse_path: quoted key", "[path]")
{
	auto p = tagforge::parse_path(R"(["weird key"])");
	REQUIRE(p.has_value());
	REQUIRE(p->size() == 1);
	REQUIRE(p->segments()[0].key == "weird key");
}

TEST_CASE("parse_path: complex Items[0].tag.display.Name", "[path]")
{
	auto p = tagforge::parse_path("Items[0].tag.display.Name");
	REQUIRE(p.has_value());
	REQUIRE(p->size() == 5);
	REQUIRE(p->segments()[0].key == "Items");
	REQUIRE(p->segments()[1].kind == PathSegment::Kind::Index);
	REQUIRE(p->segments()[1].index == 0);
	REQUIRE(p->segments()[2].key == "tag");
	REQUIRE(p->segments()[3].key == "display");
	REQUIRE(p->segments()[4].key == "Name");
}

TEST_CASE("get_at returns the Mojang-style leaf", "[path]")
{
	auto root = sample();
	auto leaf = tagforge::get_string_at(root, "Items[0].tag.display.Name");
	REQUIRE(leaf.has_value());
	REQUIRE(*leaf == "Excalibur");

	auto id1 = tagforge::get_string_at(root, "Items[1].id");
	REQUIRE(id1.has_value());
	REQUIRE(*id1 == "minecraft:bread");

	auto weird = tagforge::get_int_at(root, "[\"weird key\"]");
	REQUIRE(weird.has_value());
	REQUIRE(*weird == 7);
}

TEST_CASE("get_at: missing key", "[path][errors]")
{
	auto root = sample();
	auto leaf = tagforge::get_at(root, "Items[0].tag.display.MissingKey");
	REQUIRE_FALSE(leaf.has_value());
	REQUIRE(leaf.error().code == ErrorCode::UnknownTagId);
}

TEST_CASE("get_at: index out of range", "[path][errors]")
{
	auto root = sample();
	auto leaf = tagforge::get_at(root, "Items[42]");
	REQUIRE_FALSE(leaf.has_value());
	REQUIRE(leaf.error().code == ErrorCode::LengthOverflow);
}

TEST_CASE("get_at: wrong-type walk", "[path][errors]")
{
	auto root = sample();
	// "Items" is a List; treating it as Compound should fail.
	auto leaf = tagforge::get_at(root, "Items.tag");
	REQUIRE_FALSE(leaf.has_value());
	REQUIRE(leaf.error().code == ErrorCode::UnexpectedRootType);
}

TEST_CASE("get_byte_at / get_int_at / get_string_at honour the arm", "[path]")
{
	auto root = sample();
	auto wrong_arm = tagforge::get_int_at(root, "Items[1].id"); // it's a string
	REQUIRE_FALSE(wrong_arm.has_value());
	REQUIRE(wrong_arm.error().code == ErrorCode::UnexpectedRootType);
}
