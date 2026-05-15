// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Shared name / string decode helpers. Each dialect has a different length
// encoding (uint16 BE / uint16 LE / unsigned VarInt) and a different string
// flavour (Java uses MUTF-8, Bedrock uses plain UTF-8); these helpers fold
// the ASCII fast path + transcoding behind a uniform API.

#pragma once

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/mutf8.hpp"
#include "varint.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace tagforge::detail {

namespace name_io_detail {

[[nodiscard]] inline std::expected<std::string, Error> finish_java_string(Cursor &c, std::size_t name_len,
									  std::size_t pos_before) noexcept
{
	if (auto e = c.need(name_len); !e) {
		return std::unexpected(e.error());
	}
	std::span<const std::byte> bytes{c.data.data() + c.pos, name_len};
	c.skip(name_len);
	if (is_pure_ascii(bytes)) {
		std::string s;
		s.reserve(name_len);
		for (auto b : bytes) {
			s.push_back(static_cast<char>(b));
		}
		return s;
	}
	auto decoded = mutf8_to_utf8(bytes);
	if (!decoded) {
		Error e = decoded.error();
		e.offset += pos_before; // absolute offset of the string payload
		return std::unexpected(e);
	}
	return *decoded;
}

[[nodiscard]] inline std::expected<std::string, Error> finish_utf8_string(Cursor &c, std::size_t name_len) noexcept
{
	if (auto e = c.need(name_len); !e) {
		return std::unexpected(e.error());
	}
	std::string s(reinterpret_cast<const char *>(c.data.data() + c.pos), name_len);
	c.skip(name_len);
	return s;
}

} // namespace name_io_detail

// Java big-endian: uint16 BE length, MUTF-8 bytes. Returns UTF-8.
[[nodiscard]] inline std::expected<std::string, Error> read_string_be_mutf8(Cursor &c)
{
	const std::size_t pos_before = c.pos;
	auto len = c.read_int_be<std::uint16_t>();
	if (!len) {
		return std::unexpected(len.error());
	}
	return name_io_detail::finish_java_string(c, *len, pos_before + 2);
}

// Bedrock LE: uint16 LE length, plain UTF-8 bytes (no transcoding).
[[nodiscard]] inline std::expected<std::string, Error> read_string_le_utf8(Cursor &c)
{
	auto len = c.read_int_le<std::uint16_t>();
	if (!len) {
		return std::unexpected(len.error());
	}
	return name_io_detail::finish_utf8_string(c, *len);
}

// Bedrock VarInt: unsigned VarInt length, plain UTF-8 bytes (no transcoding).
[[nodiscard]] inline std::expected<std::string, Error> read_string_varint_utf8(Cursor &c)
{
	auto len = read_uvarint32(c);
	if (!len) {
		return std::unexpected(len.error());
	}
	return name_io_detail::finish_utf8_string(c, *len);
}

} // namespace tagforge::detail
