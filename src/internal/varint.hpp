// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// VarInt + ZigZag helpers used by the Bedrock VarInt dialect.

#pragma once

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"

#include <cstdint>
#include <expected>
#include <vector>

namespace tagforge::detail {

[[nodiscard]] inline std::expected<std::uint32_t, Error> read_uvarint32(Cursor &c)
{
	std::uint32_t out = 0;
	std::uint32_t shift = 0;
	const std::size_t start = c.pos;
	for (int i = 0; i < 5; ++i) {
		auto b = c.read_int_be<std::uint8_t>();
		if (!b) {
			return std::unexpected(b.error());
		}
		out |= static_cast<std::uint32_t>(*b & 0x7Fu) << shift;
		if ((*b & 0x80u) == 0) {
			return out;
		}
		shift += 7;
	}
	return std::unexpected(make_error(ErrorCode::InvalidVarInt, start));
}

[[nodiscard]] inline std::expected<std::uint64_t, Error> read_uvarint64(Cursor &c)
{
	std::uint64_t out = 0;
	std::uint64_t shift = 0;
	const std::size_t start = c.pos;
	for (int i = 0; i < 10; ++i) {
		auto b = c.read_int_be<std::uint8_t>();
		if (!b) {
			return std::unexpected(b.error());
		}
		out |= static_cast<std::uint64_t>(*b & 0x7Fu) << shift;
		if ((*b & 0x80u) == 0) {
			return out;
		}
		shift += 7;
	}
	return std::unexpected(make_error(ErrorCode::InvalidVarInt, start));
}

[[nodiscard]] inline std::expected<std::int32_t, Error> read_zigzag32(Cursor &c)
{
	auto u = read_uvarint32(c);
	if (!u) {
		return std::unexpected(u.error());
	}
	return static_cast<std::int32_t>((*u >> 1) ^ -static_cast<std::int32_t>(*u & 1u));
}

[[nodiscard]] inline std::expected<std::int64_t, Error> read_zigzag64(Cursor &c)
{
	auto u = read_uvarint64(c);
	if (!u) {
		return std::unexpected(u.error());
	}
	return static_cast<std::int64_t>((*u >> 1) ^ -static_cast<std::int64_t>(*u & 1ull));
}

inline void write_uvarint32(std::vector<std::byte> &out, std::uint32_t v)
{
	while (v >= 0x80u) {
		out.push_back(static_cast<std::byte>((v & 0x7Fu) | 0x80u));
		v >>= 7;
	}
	out.push_back(static_cast<std::byte>(v));
}

inline void write_uvarint64(std::vector<std::byte> &out, std::uint64_t v)
{
	while (v >= 0x80ull) {
		out.push_back(static_cast<std::byte>((v & 0x7Fu) | 0x80u));
		v >>= 7;
	}
	out.push_back(static_cast<std::byte>(v));
}

inline void write_zigzag32(std::vector<std::byte> &out, std::int32_t v)
{
	const std::uint32_t u = (static_cast<std::uint32_t>(v) << 1) ^ static_cast<std::uint32_t>(v >> 31);
	write_uvarint32(out, u);
}

inline void write_zigzag64(std::vector<std::byte> &out, std::int64_t v)
{
	const std::uint64_t u = (static_cast<std::uint64_t>(v) << 1) ^ static_cast<std::uint64_t>(v >> 63);
	write_uvarint64(out, u);
}

} // namespace tagforge::detail
