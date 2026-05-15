// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/cursor.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

using tagforge::Cursor;
using tagforge::ErrorCode;

namespace {

constexpr std::byte b(std::uint8_t v)
{
	return static_cast<std::byte>(v);
}

template<std::size_t N> constexpr std::array<std::byte, N> make(std::initializer_list<std::uint8_t> bytes)
{
	std::array<std::byte, N> arr{};
	std::size_t i = 0;
	for (auto v : bytes) {
		arr[i++] = static_cast<std::byte>(v);
	}
	return arr;
}

} // namespace

TEST_CASE("Cursor::need rejects past-end reads", "[cursor]")
{
	std::array<std::byte, 4> data{b(0), b(1), b(2), b(3)};
	Cursor c{data, 2};
	REQUIRE(c.need(2).has_value());
	REQUIRE_FALSE(c.need(3).has_value());
	REQUIRE(c.need(3).error().code == ErrorCode::UnexpectedEndOfInput);
	REQUIRE(c.need(3).error().offset == 2);
}

TEST_CASE("Cursor::take advances pos", "[cursor]")
{
	std::array<std::byte, 4> data{b(0xAA), b(0xBB), b(0xCC), b(0xDD)};
	Cursor c{data, 0};
	auto two = c.take(2);
	REQUIRE(two.size() == 2);
	REQUIRE(c.pos == 2);
	REQUIRE(c.remaining() == 2);
}

TEST_CASE("read_int_be reads big-endian integers", "[cursor]")
{
	std::array<std::byte, 8> data{b(0x12), b(0x34), b(0x56), b(0x78), b(0x9A), b(0xBC), b(0xDE), b(0xF0)};
	Cursor c{data, 0};

	auto a = c.read_int_be<std::int16_t>();
	REQUIRE(a.has_value());
	REQUIRE(*a == static_cast<std::int16_t>(0x1234));

	auto b32 = c.read_int_be<std::int32_t>();
	REQUIRE(b32.has_value());
	REQUIRE(static_cast<std::uint32_t>(*b32) == 0x56789ABCU);
}

TEST_CASE("read_int_le reads little-endian integers", "[cursor]")
{
	std::array<std::byte, 4> data{b(0x78), b(0x56), b(0x34), b(0x12)};
	Cursor c{data, 0};
	auto v = c.read_int_le<std::int32_t>();
	REQUIRE(v.has_value());
	REQUIRE(static_cast<std::uint32_t>(*v) == 0x12345678U);
}

TEST_CASE("read_int_* short-input returns UnexpectedEndOfInput", "[cursor]")
{
	std::array<std::byte, 3> data{b(0), b(0), b(0)};
	Cursor c{data, 0};
	auto v = c.read_int_be<std::int32_t>();
	REQUIRE_FALSE(v.has_value());
	REQUIRE(v.error().code == ErrorCode::UnexpectedEndOfInput);
}

TEST_CASE("read_float_be / read_double_be bit-cast correctly", "[cursor]")
{
	// 1.0f big-endian -> 0x3F800000
	std::array<std::byte, 4> f_data{b(0x3F), b(0x80), b(0x00), b(0x00)};
	Cursor cf{f_data, 0};
	auto f = cf.read_float_be();
	REQUIRE(f.has_value());
	REQUIRE(*f == 1.0f);

	// 1.0 big-endian -> 0x3FF0000000000000
	std::array<std::byte, 8> d_data{b(0x3F), b(0xF0), b(0x00), b(0x00), b(0x00), b(0x00), b(0x00), b(0x00)};
	Cursor cd{d_data, 0};
	auto d = cd.read_double_be();
	REQUIRE(d.has_value());
	REQUIRE(*d == 1.0);
}

TEST_CASE("read_float_le / read_double_le bit-cast correctly", "[cursor]")
{
	std::array<std::byte, 4> f_data{b(0x00), b(0x00), b(0x80), b(0x3F)};
	Cursor cf{f_data, 0};
	auto f = cf.read_float_le();
	REQUIRE(f.has_value());
	REQUIRE(*f == 1.0f);

	std::array<std::byte, 8> d_data{b(0x00), b(0x00), b(0x00), b(0x00), b(0x00), b(0x00), b(0xF0), b(0x3F)};
	Cursor cd{d_data, 0};
	auto d = cd.read_double_le();
	REQUIRE(d.has_value());
	REQUIRE(*d == 1.0);
}
