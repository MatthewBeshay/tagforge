// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/format.hpp"
#include "tagforge/view.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using tagforge::CompoundView;
using tagforge::ErrorCode;
using tagforge::Format;
using tagforge::ListView;
using tagforge::TagId;
using tagforge::View;

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

TEST_CASE("View::decode reads the bigtest root", "[view][java][bigtest]")
{
	const auto data = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto root = View::decode(data, Format::JavaNamedRoot);
	REQUIRE(root.has_value());
	REQUIRE(root->kind() == TagId::Compound);
	REQUIRE(root->name_utf8() == "Level");
}

TEST_CASE("CompoundView::find locates a primitive child by name", "[view][java][bigtest]")
{
	const auto data = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto root = View::decode(data, Format::JavaNamedRoot);
	REQUIRE(root.has_value());

	auto cv = root->as_compound();
	REQUIRE(cv.has_value());

	auto byte_test = cv->find("byteTest");
	REQUIRE(byte_test.has_value());
	REQUIRE(byte_test->kind() == TagId::Byte);
	auto b = byte_test->as_byte();
	REQUIRE(b.has_value());
	REQUIRE(*b == static_cast<std::int8_t>(127));

	auto long_test = cv->find("longTest");
	REQUIRE(long_test.has_value());
	REQUIRE(long_test->kind() == TagId::Long);
	REQUIRE(*long_test->as_long() == 9223372036854775807LL);
}

TEST_CASE("CompoundView::find returns nullopt for missing keys", "[view][java]")
{
	const auto data = read_file(std::filesystem::path{kFixtureDir} / "java" / "bigtest_raw.nbt");
	auto root = View::decode(data, Format::JavaNamedRoot);
	auto cv = root->as_compound();
	REQUIRE_FALSE(cv->find("definitely_not_a_key").has_value());
}

TEST_CASE("ListView yields the announced arm and size", "[view][java]")
{
	// Anonymous-root List<Byte> count=3 [1,2,3].
	const auto data = bytes({0x09,
				 0x01, // child = Byte
				 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03});
	auto root = View::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE(root.has_value());
	REQUIRE(root->kind() == TagId::List);

	auto list = root->as_list();
	REQUIRE(list.has_value());
	REQUIRE(list->arm() == TagId::Byte);
	REQUIRE(list->size() == 3);

	auto first = list->at(0);
	REQUIRE(first.has_value());
	REQUIRE(*first->as_byte() == 1);

	auto last = list->at(2);
	REQUIRE(last.has_value());
	REQUIRE(*last->as_byte() == 3);

	auto oob = list->at(3);
	REQUIRE_FALSE(oob.has_value());
	REQUIRE(oob.error().code == ErrorCode::LengthOverflow);
}

TEST_CASE("CompoundView iterator visits children in wire order", "[view][java]")
{
	// Anonymous-root Compound { a:int=1, b:int=2, c:int=3 }
	const auto data = bytes({0x0A, 0x03, 0x00, 0x01, 'a',  0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 'b',
				 0x00, 0x00, 0x00, 0x02, 0x03, 0x00, 0x01, 'c',  0x00, 0x00, 0x00, 0x03, 0x00});
	auto root = View::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE(root.has_value());
	auto cv = root->as_compound();
	REQUIRE(cv.has_value());

	std::vector<std::string> names;
	std::vector<std::int32_t> vals;
	for (auto &child : *cv) {
		names.push_back(child.name);
		vals.push_back(*child.child.as_int());
	}
	REQUIRE(names == std::vector<std::string>{"a", "b", "c"});
	REQUIRE(vals == std::vector<std::int32_t>{1, 2, 3});
}

TEST_CASE("View::decode_anonymous handles the bare 0x00 sentinel", "[view][java]")
{
	const auto data = bytes({0x00});
	auto root = View::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE(root.has_value());
	REQUIRE(root->kind() == TagId::End);
}

TEST_CASE("View::materialise reproduces the owning tree", "[view][java]")
{
	const auto data = bytes({0x03, 0x00, 0x00, 0x00, 0x2A}); // anon Int = 42
	auto root = View::decode_anonymous(data, Format::JavaAnonymousRoot);
	REQUIRE(root.has_value());
	auto v = root->materialise();
	REQUIRE(v.has_value());
	REQUIRE(v->kind() == TagId::Int);
	REQUIRE(std::get<std::int32_t>(v->v) == 42);
}
