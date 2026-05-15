// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/tag_id.hpp"

using tagforge::TagId;
using tagforge::tag_id_is_valid;
using tagforge::tag_id_name;

TEST_CASE("TagId raw values match the wire protocol", "[tag_id]")
{
	STATIC_REQUIRE(static_cast<int>(TagId::End) == 0);
	STATIC_REQUIRE(static_cast<int>(TagId::Byte) == 1);
	STATIC_REQUIRE(static_cast<int>(TagId::Short) == 2);
	STATIC_REQUIRE(static_cast<int>(TagId::Int) == 3);
	STATIC_REQUIRE(static_cast<int>(TagId::Long) == 4);
	STATIC_REQUIRE(static_cast<int>(TagId::Float) == 5);
	STATIC_REQUIRE(static_cast<int>(TagId::Double) == 6);
	STATIC_REQUIRE(static_cast<int>(TagId::ByteArray) == 7);
	STATIC_REQUIRE(static_cast<int>(TagId::String) == 8);
	STATIC_REQUIRE(static_cast<int>(TagId::List) == 9);
	STATIC_REQUIRE(static_cast<int>(TagId::Compound) == 10);
	STATIC_REQUIRE(static_cast<int>(TagId::IntArray) == 11);
	STATIC_REQUIRE(static_cast<int>(TagId::LongArray) == 12);
}

TEST_CASE("tag_id_is_valid covers 0..12 inclusive", "[tag_id]")
{
	for (int i = 0; i <= 12; ++i) {
		REQUIRE(tag_id_is_valid(static_cast<std::uint8_t>(i)));
	}
	REQUIRE_FALSE(tag_id_is_valid(13));
	REQUIRE_FALSE(tag_id_is_valid(0xFF));
}

TEST_CASE("tag_id_name returns canonical names", "[tag_id]")
{
	REQUIRE(tag_id_name(TagId::End) == "End");
	REQUIRE(tag_id_name(TagId::Byte) == "Byte");
	REQUIRE(tag_id_name(TagId::Compound) == "Compound");
	REQUIRE(tag_id_name(TagId::LongArray) == "LongArray");
}
