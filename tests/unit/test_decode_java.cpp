// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "tagforge/decode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

using tagforge::Compound;
using tagforge::ErrorCode;
using tagforge::Format;
using tagforge::List;
using tagforge::TagId;
using tagforge::Value;

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

TEST_CASE("decode bigtest_raw.nbt produces the documented tree", "[decode][java][bigtest]")
{
	const auto data = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");

	auto result = tagforge::decode(data, Format::JavaNamedRoot);
	REQUIRE(result.has_value());

	const auto &nv = *result;
	REQUIRE(nv.name == "Level");
	REQUIRE(nv.value.kind() == TagId::Compound);

	const Compound &root = std::get<Compound>(nv.value.v);

	// A handful of well-known bigtest entries.
	REQUIRE(*tagforge::get_long(root, "longTest") == 9223372036854775807LL);
	REQUIRE(*tagforge::get_short(root, "shortTest") == static_cast<std::int16_t>(32767));
	REQUIRE(*tagforge::get_byte(root, "byteTest") == static_cast<std::int8_t>(127));
	REQUIRE(*tagforge::get_string(root, "stringTest") ==
		"HELLO WORLD THIS IS A TEST STRING \xC3\x85\xC3\x84\xC3\x96!");
	REQUIRE(*tagforge::get_float(root, "floatTest") == Catch::Approx(0.49823147058486938));
	REQUIRE(*tagforge::get_double(root, "doubleTest") == Catch::Approx(0.4931287132182315));

	// listTest (long) is a List<Long> of [11, 12, 13, 14, 15].
	auto lst = tagforge::get_list(root, "listTest (long)");
	REQUIRE(lst.has_value());
	const List &ll = **lst;
	REQUIRE(ll.size() == 5);
	REQUIRE(std::get<std::int64_t>(ll[0].v) == 11);
	REQUIRE(std::get<std::int64_t>(ll[4].v) == 15);

	// byteArrayTest is generated procedurally; size = 1000.
	auto ba = tagforge::get_byte_array(root, "byteArrayTest (the first 1000 values of (n*n*255+n*7)%100, "
						 "starting with n=0 (0, 62, 34, 16, 8, ...))");
	REQUIRE(ba.has_value());
	REQUIRE(ba->size() == 1000);
	REQUIRE((*ba)[0] == 0);
	REQUIRE((*ba)[1] == 62);
}

TEST_CASE("decode_anonymous handles the bare 0x00 sentinel", "[decode][java]")
{
	const auto data = bytes({0x00});
	auto out = tagforge::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE(out.has_value());
	REQUIRE(out->kind() == TagId::End);
}

TEST_CASE("decode_anonymous Int", "[decode][java]")
{
	const auto data = bytes({0x03, 0x00, 0x00, 0x00, 0x2A});
	auto out = tagforge::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE(out.has_value());
	REQUIRE(out->kind() == TagId::Int);
	REQUIRE(std::get<std::int32_t>(out->v) == 42);
}

TEST_CASE("decode anonymous compound with mixed children", "[decode][java]")
{
	// Anonymous-root Compound { x:int=1, s:str="hi", l:list<byte>[1,2] }
	const auto data = bytes({
		0x0A,                                          // open Compound
		0x03, 0x00, 0x01, 'x',                         // Int "x"
		0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x01, 's', // String "s" = "hi"
		0x00, 0x02, 'h',  'i',  0x09, 0x00, 0x01, 'l', // List "l" of Byte
		0x01, 0x00, 0x00, 0x00, 0x02, 0x01, 0x02,
		0x00, // close Compound
	});

	auto out = tagforge::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE(out.has_value());
	REQUIRE(out->kind() == TagId::Compound);
	const auto &c = std::get<Compound>(out->v);
	REQUIRE(c.size() == 3);
	REQUIRE(*tagforge::get_int(c, "x") == 1);
	REQUIRE(*tagforge::get_string(c, "s") == "hi");
	auto lst = tagforge::get_list(c, "l");
	REQUIRE(lst.has_value());
	REQUIRE((*lst)->size() == 2);
	REQUIRE(std::get<std::int8_t>((**lst)[0].v) == 1);
	REQUIRE(std::get<std::int8_t>((**lst)[1].v) == 2);
}

TEST_CASE("decode List<End, 0> yields an empty List", "[decode][java]")
{
	const auto data = bytes({0x09, 0x00, 0x00, 0x00, 0x00, 0x00});
	auto out = tagforge::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE(out.has_value());
	REQUIRE(out->kind() == TagId::List);
	REQUIRE(std::get<List>(out->v).empty());
}

TEST_CASE("decode IntArray and LongArray", "[decode][java]")
{
	// IntArray length=2, [0x00000001, 0x00000002]
	const auto ia = bytes({0x0B, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02});
	auto out = tagforge::decode_anonymous(ia, Format::JavaAnonymousRoot);
	REQUIRE(out.has_value());
	REQUIRE(out->kind() == TagId::IntArray);
	const auto &v = std::get<std::vector<std::int32_t>>(out->v);
	REQUIRE(v.size() == 2);
	REQUIRE(v[0] == 1);
	REQUIRE(v[1] == 2);

	const auto la = bytes({0x0C, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF});
	auto out2 = tagforge::decode_anonymous(la, Format::JavaAnonymousRoot);
	REQUIRE(out2.has_value());
	REQUIRE(out2->kind() == TagId::LongArray);
	const auto &v2 = std::get<std::vector<std::int64_t>>(out2->v);
	REQUIRE(v2.size() == 1);
	REQUIRE(v2[0] == 0xFF);
}

TEST_CASE("decode propagates negative-length errors", "[decode][java][errors]")
{
	const auto data = bytes({0x07, 0xFF, 0xFF, 0xFF, 0xFF});
	auto out = tagforge::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::NegativeLength);
}

TEST_CASE("decode propagates invalid MUTF-8 names", "[decode][java][errors]")
{
	// Named root Compound with a name containing bare 0x00 -> invalid MUTF-8.
	const auto data = bytes({0x0A, 0x00, 0x03, 'a', 0x00, 'b', 0x00});
	auto out = tagforge::decode(data, Format::JavaNamedRoot);
	REQUIRE_FALSE(out.has_value());
	REQUIRE(out.error().code == ErrorCode::InvalidMutf8);
}
