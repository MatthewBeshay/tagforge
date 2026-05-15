// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/error.hpp"

using tagforge::Error;
using tagforge::ErrorCode;

TEST_CASE("error_code_name returns a label for every code", "[error]")
{
	REQUIRE(tagforge::error_code_name(ErrorCode::Ok) == "Ok");
	REQUIRE(tagforge::error_code_name(ErrorCode::UnexpectedEndOfInput) == "UnexpectedEndOfInput");
	REQUIRE(tagforge::error_code_name(ErrorCode::InvalidMutf8) == "InvalidMutf8");
	REQUIRE(tagforge::error_code_name(ErrorCode::RegionInvalidChunk) == "RegionInvalidChunk");
}

TEST_CASE("Error::message falls back to the code name when detail is empty", "[error]")
{
	Error e{ErrorCode::UnknownTagId, 42, {}};
	REQUIRE(e.message() == "UnknownTagId");
}

TEST_CASE("Error::message prefers detail when present", "[error]")
{
	Error e{ErrorCode::SnbtSyntax, 17, "unterminated string"};
	REQUIRE(e.message() == "unterminated string");
}

TEST_CASE("Error supports equality", "[error]")
{
	Error a{ErrorCode::InvalidUtf8, 5, "x"};
	Error b{ErrorCode::InvalidUtf8, 5, "x"};
	Error c{ErrorCode::InvalidUtf8, 6, "x"};
	REQUIRE(a == b);
	REQUIRE_FALSE(a == c);
}
