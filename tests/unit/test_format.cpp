// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/format.hpp"

using tagforge::Format;

TEST_CASE("format_name returns the expected label for each dialect", "[format]")
{
	REQUIRE(tagforge::format_name(Format::JavaNamedRoot) == "JavaNamedRoot");
	REQUIRE(tagforge::format_name(Format::JavaAnonymousRoot) == "JavaAnonymousRoot");
	REQUIRE(tagforge::format_name(Format::BedrockLittleEndian) == "BedrockLittleEndian");
	REQUIRE(tagforge::format_name(Format::BedrockVarInt) == "BedrockVarInt");
}

TEST_CASE("Format predicates classify dialects", "[format]")
{
	STATIC_REQUIRE(tagforge::is_big_endian(Format::JavaNamedRoot));
	STATIC_REQUIRE(tagforge::is_big_endian(Format::JavaAnonymousRoot));
	STATIC_REQUIRE(!tagforge::is_big_endian(Format::BedrockLittleEndian));
	STATIC_REQUIRE(!tagforge::is_big_endian(Format::BedrockVarInt));

	STATIC_REQUIRE(tagforge::has_root_name(Format::JavaNamedRoot));
	STATIC_REQUIRE(!tagforge::has_root_name(Format::JavaAnonymousRoot));
	STATIC_REQUIRE(tagforge::has_root_name(Format::BedrockLittleEndian));
	STATIC_REQUIRE(tagforge::has_root_name(Format::BedrockVarInt));

	STATIC_REQUIRE(!tagforge::uses_varint(Format::JavaNamedRoot));
	STATIC_REQUIRE(!tagforge::uses_varint(Format::JavaAnonymousRoot));
	STATIC_REQUIRE(!tagforge::uses_varint(Format::BedrockLittleEndian));
	STATIC_REQUIRE(tagforge::uses_varint(Format::BedrockVarInt));
}
