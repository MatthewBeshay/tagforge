// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/mutf8.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using tagforge::ErrorCode;

namespace {

std::vector<std::byte> bytes(std::initializer_list<std::uint8_t> il)
{
	std::vector<std::byte> v;
	v.reserve(il.size());
	for (auto x : il) {
		v.push_back(static_cast<std::byte>(x));
	}
	return v;
}

std::string str_from_bytes(std::span<const std::byte> s)
{
	std::string out;
	out.reserve(s.size());
	for (auto x : s) {
		out.push_back(static_cast<char>(x));
	}
	return out;
}

} // namespace

TEST_CASE("is_pure_ascii distinguishes ASCII from MUTF-8 multibyte", "[mutf8]")
{
	REQUIRE(tagforge::is_pure_ascii(std::string_view{"hello world"}));
	REQUIRE_FALSE(tagforge::is_pure_ascii(std::string_view{"naïve"}));
	// NUL not pure ASCII because MUTF-8 special-cases it.
	REQUIRE_FALSE(tagforge::is_pure_ascii(std::string_view{std::string("a\0b", 3)}));
}

TEST_CASE("mutf8_to_utf8 passes through pure ASCII unchanged", "[mutf8]")
{
	auto out = tagforge::mutf8_to_utf8("hello");
	REQUIRE(out.has_value());
	REQUIRE(*out == "hello");
}

TEST_CASE("mutf8_to_utf8 decodes 0xC0 0x80 -> U+0000", "[mutf8]")
{
	auto in = bytes({0x41, 0xC0, 0x80, 0x42}); // "A" NUL "B"
	auto out = tagforge::mutf8_to_utf8(in);
	REQUIRE(out.has_value());
	REQUIRE(out->size() == 3);
	REQUIRE((*out)[0] == 'A');
	REQUIRE((*out)[1] == '\0');
	REQUIRE((*out)[2] == 'B');
}

TEST_CASE("mutf8_to_utf8 rejects bare 0x00", "[mutf8]")
{
	auto in = bytes({0x41, 0x00, 0x42});
	auto out = tagforge::mutf8_to_utf8(in);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::InvalidMutf8);
	REQUIRE(out.error().offset == 1);
}

TEST_CASE("mutf8_to_utf8 decodes BMP 2-byte sequences (Latin-1 supplement)", "[mutf8]")
{
	// U+00E9 LATIN SMALL LETTER E WITH ACUTE -> 0xC3 0xA9 in both UTF-8 and MUTF-8.
	auto in = bytes({0xC3, 0xA9});
	auto out = tagforge::mutf8_to_utf8(in);
	REQUIRE(out.has_value());
	REQUIRE(out->size() == 2);
	REQUIRE(static_cast<std::uint8_t>((*out)[0]) == 0xC3);
	REQUIRE(static_cast<std::uint8_t>((*out)[1]) == 0xA9);
}

TEST_CASE("mutf8_to_utf8 decodes BMP 3-byte sequences", "[mutf8]")
{
	// U+3042 HIRAGANA LETTER A -> 0xE3 0x81 0x82.
	auto in = bytes({0xE3, 0x81, 0x82});
	auto out = tagforge::mutf8_to_utf8(in);
	REQUIRE(out.has_value());
	REQUIRE(out->size() == 3);
}

TEST_CASE("mutf8_to_utf8 merges surrogate-pair-of-3-byte into a 4-byte UTF-8 sequence", "[mutf8]")
{
	// U+1F600 GRINNING FACE.
	// MUTF-8: high surrogate U+D83D = 0xED 0xA0 0xBD ; low surrogate U+DE00 = 0xED 0xB8 0x80.
	// UTF-8 4-byte: 0xF0 0x9F 0x98 0x80.
	auto in = bytes({0xED, 0xA0, 0xBD, 0xED, 0xB8, 0x80});
	auto out = tagforge::mutf8_to_utf8(in);
	REQUIRE(out.has_value());
	REQUIRE(out->size() == 4);
	REQUIRE(static_cast<std::uint8_t>((*out)[0]) == 0xF0);
	REQUIRE(static_cast<std::uint8_t>((*out)[1]) == 0x9F);
	REQUIRE(static_cast<std::uint8_t>((*out)[2]) == 0x98);
	REQUIRE(static_cast<std::uint8_t>((*out)[3]) == 0x80);
}

TEST_CASE("mutf8_to_utf8 rejects lone surrogates", "[mutf8]")
{
	auto in = bytes({0xED, 0xA0, 0xBD, 0x41}); // high surrogate then ASCII
	auto out = tagforge::mutf8_to_utf8(in);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::InvalidMutf8);
}

TEST_CASE("mutf8_to_utf8 rejects 4-byte UTF-8 sequences", "[mutf8]")
{
	// 0xF0 0x9F 0x98 0x80 is valid UTF-8 for U+1F600 but disallowed in MUTF-8.
	auto in = bytes({0xF0, 0x9F, 0x98, 0x80});
	auto out = tagforge::mutf8_to_utf8(in);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::InvalidMutf8);
}

TEST_CASE("mutf8_to_utf8 rejects overlong 2-byte sequences", "[mutf8]")
{
	auto in = bytes({0xC1, 0x80}); // would decode to U+0040 ('@'); disallowed
	auto out = tagforge::mutf8_to_utf8(in);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::InvalidMutf8);
}

TEST_CASE("utf8_to_mutf8 round-trips ASCII unchanged", "[mutf8]")
{
	auto out = tagforge::utf8_to_mutf8("hello");
	REQUIRE(out.has_value());
	REQUIRE(*out == "hello");
}

TEST_CASE("utf8_to_mutf8 encodes NUL as 0xC0 0x80", "[mutf8]")
{
	auto out = tagforge::utf8_to_mutf8(std::string_view{std::string("A\0B", 3)});
	REQUIRE(out.has_value());
	REQUIRE(out->size() == 4);
	REQUIRE((*out)[0] == 'A');
	REQUIRE(static_cast<std::uint8_t>((*out)[1]) == 0xC0);
	REQUIRE(static_cast<std::uint8_t>((*out)[2]) == 0x80);
	REQUIRE((*out)[3] == 'B');
}

TEST_CASE("utf8_to_mutf8 splits 4-byte UTF-8 into 6-byte MUTF-8 surrogate pair", "[mutf8]")
{
	std::string utf8;
	utf8.push_back(static_cast<char>(0xF0));
	utf8.push_back(static_cast<char>(0x9F));
	utf8.push_back(static_cast<char>(0x98));
	utf8.push_back(static_cast<char>(0x80));
	auto out = tagforge::utf8_to_mutf8(utf8);
	REQUIRE(out.has_value());
	REQUIRE(out->size() == 6);
	REQUIRE(static_cast<std::uint8_t>((*out)[0]) == 0xED);
	REQUIRE(static_cast<std::uint8_t>((*out)[3]) == 0xED);
}

TEST_CASE("utf8_to_mutf8 rejects surrogate codepoints in UTF-8 input", "[mutf8]")
{
	// U+D83D as 3-byte UTF-8 = 0xED 0xA0 0xBD. Not valid UTF-8.
	auto in_str = std::string{static_cast<char>(0xED), static_cast<char>(0xA0), static_cast<char>(0xBD)};
	auto out = tagforge::utf8_to_mutf8(in_str);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::InvalidUtf8);
}

TEST_CASE("Round-trip: utf8 -> mutf8 -> utf8 preserves bytes", "[mutf8]")
{
	for (std::string_view sample :
	     {std::string_view{"hello"}, std::string_view{"naïve"}, std::string_view{"日本語"}}) {
		auto enc = tagforge::utf8_to_mutf8(sample);
		REQUIRE(enc.has_value());
		auto dec = tagforge::mutf8_to_utf8(*enc);
		REQUIRE(dec.has_value());
		REQUIRE(*dec == std::string{sample});
	}

	// NUL byte: roundtrips through MUTF-8 NUL marker.
	std::string with_nul("a\0b", 3);
	auto enc = tagforge::utf8_to_mutf8(with_nul);
	REQUIRE(enc.has_value());
	auto dec = tagforge::mutf8_to_utf8(*enc);
	REQUIRE(dec.has_value());
	REQUIRE(*dec == with_nul);

	// Supplementary plane.
	std::string smile = "\xF0\x9F\x98\x80";
	auto enc2 = tagforge::utf8_to_mutf8(smile);
	REQUIRE(enc2.has_value());
	REQUIRE(enc2->size() == 6);
	auto dec2 = tagforge::mutf8_to_utf8(*enc2);
	REQUIRE(dec2.has_value());
	REQUIRE(*dec2 == smile);
}
