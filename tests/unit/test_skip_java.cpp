// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/format.hpp"
#include "tagforge/skip.hpp"
#include "tagforge/tag_id.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

using tagforge::ErrorCode;
using tagforge::Format;
using tagforge::TagId;

namespace {

std::vector<std::byte> read_file(const std::filesystem::path &p)
{
	std::ifstream f(p, std::ios::binary | std::ios::ate);
	REQUIRE(f.is_open());
	const std::streamsize n = f.tellg();
	f.seekg(0);
	std::vector<std::byte> out(static_cast<std::size_t>(n));
	f.read(reinterpret_cast<char *>(out.data()), n);
	return out;
}

std::vector<std::byte> bytes(std::initializer_list<std::uint8_t> il)
{
	std::vector<std::byte> out;
	out.reserve(il.size());
	for (auto v : il) {
		out.push_back(static_cast<std::byte>(v));
	}
	return out;
}

constexpr const char *kFixtureDir = TAGFORGE_FIXTURE_DIR;

} // namespace

TEST_CASE("skip(JavaNamedRoot) consumes every byte of bigtest_raw.nbt", "[skip][java]")
{
	const auto data = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	REQUIRE_FALSE(data.empty());

	auto consumed = tagforge::skip(data, Format::JavaNamedRoot);
	REQUIRE(consumed.has_value());
	REQUIRE(*consumed == data.size());
}

TEST_CASE("skip_expect on bigtest enforces Compound root", "[skip][java]")
{
	const auto data = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");

	auto ok = tagforge::skip_expect(data, Format::JavaNamedRoot, TagId::Compound);
	REQUIRE(ok.has_value());
	REQUIRE(*ok == data.size());

	auto bad = tagforge::skip_expect(data, Format::JavaNamedRoot, TagId::List);
	REQUIRE_FALSE(bad.has_value());
	REQUIRE(bad.error().code == ErrorCode::UnexpectedRootType);
}

TEST_CASE("skip(JavaAnonymousRoot) accepts bare 0x00 as the no-NBT sentinel", "[skip][java]")
{
	const auto data = bytes({0x00});
	auto consumed = tagforge::skip(data, Format::JavaAnonymousRoot);
	REQUIRE(consumed.has_value());
	REQUIRE(*consumed == 1);
}

TEST_CASE("skip(JavaNamedRoot) accepts bare 0x00", "[skip][java]")
{
	const auto data = bytes({0x00});
	auto consumed = tagforge::skip(data, Format::JavaNamedRoot);
	REQUIRE(consumed.has_value());
	REQUIRE(*consumed == 1);
}

TEST_CASE("skip(JavaAnonymousRoot) consumes empty compound (0x0A 0x00)", "[skip][java]")
{
	const auto data = bytes({0x0A, 0x00});
	auto consumed = tagforge::skip(data, Format::JavaAnonymousRoot);
	REQUIRE(consumed.has_value());
	REQUIRE(*consumed == 2);
}

TEST_CASE("skip handles List<End, 0> degenerate", "[skip][java]")
{
	// anonymous-root List<End, length=0>:
	//   tag: 0x09 (List)
	//   childType: 0x00 (End)
	//   length: 0x00 0x00 0x00 0x00
	const auto data = bytes({0x09, 0x00, 0x00, 0x00, 0x00, 0x00});
	auto consumed = tagforge::skip(data, Format::JavaAnonymousRoot);
	REQUIRE(consumed.has_value());
	REQUIRE(*consumed == 6);
}

TEST_CASE("skip handles List<End, N>; payload-less elements", "[skip][java]")
{
	// anonymous-root List<End, length=5>: no element payloads should follow.
	const auto data = bytes({0x09, 0x00, 0x00, 0x00, 0x00, 0x05});
	auto consumed = tagforge::skip(data, Format::JavaAnonymousRoot);
	REQUIRE(consumed.has_value());
	REQUIRE(*consumed == 6);
}

TEST_CASE("skip rejects negative array lengths", "[skip][java]")
{
	// anonymous-root ByteArray with length = -1.
	const auto data = bytes({0x07, 0xFF, 0xFF, 0xFF, 0xFF});
	auto out = tagforge::skip(data, Format::JavaAnonymousRoot);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::NegativeLength);
}

TEST_CASE("skip rejects unknown tag IDs", "[skip][java]")
{
	const auto data = bytes({0xFE}); // 0xFE > 12
	auto out = tagforge::skip(data, Format::JavaAnonymousRoot);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::UnknownTagId);
}

TEST_CASE("skip rejects truncated input", "[skip][java]")
{
	// anonymous-root TAG_Int but only 3 of 4 payload bytes present.
	const auto data = bytes({0x03, 0x00, 0x00, 0x00});
	auto out = tagforge::skip(data, Format::JavaAnonymousRoot);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::UnexpectedEndOfInput);
}

TEST_CASE("skip handles a hand-rolled named root compound with mixed children", "[skip][java]")
{
	// JavaNamedRoot:
	//   0x0A Compound
	//   00 04 "root"
	//   children:
	//     0x03 Int "x" = 0x00000001
	//     0x08 String "s" = "hi"
	//     0x09 List "l" of Byte, count=2, [0x01, 0x02]
	//     0x0B IntArray "ia" length=1, [0x00 0x00 0x00 0x07]
	//   0x00 End
	const auto data = bytes({
		0x0A,                              // compound tag
		0x00, 0x04, 'r',  'o',  'o',  't', // root name "root"
		0x03,                              // Int
		0x00, 0x01, 'x',  0x00, 0x00, 0x00, 0x01,
		0x08, // String
		0x00, 0x01, 's',  0x00, 0x02, 'h',  'i',
		0x09, // List
		0x00, 0x01, 'l',
		0x01,                                           // child tag = Byte
		0x00, 0x00, 0x00, 0x02,                         // count = 2
		0x01, 0x02,                                     // payload
		0x0B,                                           // IntArray
		0x00, 0x02, 'i',  'a',  0x00, 0x00, 0x00, 0x01, // length = 1
		0x00, 0x00, 0x00, 0x07,                         // [7]
		0x00,                                           // End of compound
	});

	auto out = tagforge::skip(data, Format::JavaNamedRoot);
	REQUIRE(out.has_value());
	REQUIRE(*out == data.size());
}

TEST_CASE("Bedrock skippers accept their own writer output", "[skip][bedrock]")
{
	// Minimal Bedrock LE named-root payload: Compound named "" with TAG_End body.
	// Wire: 0x0A (Compound) 0x00 0x00 (uint16 LE name length = 0) 0x00 (TAG_End).
	const auto le_data = bytes({0x0A, 0x00, 0x00, 0x00});
	auto a = tagforge::skip(le_data, Format::BedrockLittleEndian);
	REQUIRE(a.has_value());
	REQUIRE(*a == le_data.size());

	// Minimal Bedrock VarInt: 0x0A (Compound) 0x00 (uvarint name length = 0) 0x00 (TAG_End).
	const auto vi_data = bytes({0x0A, 0x00, 0x00});
	auto b = tagforge::skip(vi_data, Format::BedrockVarInt);
	REQUIRE(b.has_value());
	REQUIRE(*b == vi_data.size());
}
