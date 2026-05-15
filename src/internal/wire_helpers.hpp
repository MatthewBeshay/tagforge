// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Inline wire-format helpers shared by every skip / decode / encode / view
// translation unit. Centralises the per-dialect length-prefix decoding so
// that the individual dialect TUs only carry their unique payload logic.

#pragma once

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"
#include "varint.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>

namespace tagforge::detail {

// Read a single TagId byte, validating it. Single byte, format-agnostic.
[[nodiscard]] inline std::expected<TagId, Error> read_tag_byte(Cursor &c) noexcept
{
	const std::size_t pos_before = c.pos;
	auto raw = c.read_int_be<std::uint8_t>();
	if (!raw) {
		return std::unexpected(raw.error());
	}
	if (!tag_id_is_valid(*raw)) {
		return std::unexpected(make_error(ErrorCode::UnknownTagId, pos_before));
	}
	return static_cast<TagId>(*raw);
}

// Read an array length (int32 BE for Java dialects). Rejects negatives.
[[nodiscard]] inline std::expected<std::int32_t, Error> read_be_array_length(Cursor &c) noexcept
{
	const std::size_t pos_before = c.pos;
	auto len = c.read_int_be<std::int32_t>();
	if (!len) {
		return std::unexpected(len.error());
	}
	if (*len < 0) {
		return std::unexpected(make_error(ErrorCode::NegativeLength, pos_before));
	}
	return *len;
}

// Read an array length (int32 LE for Bedrock LE). Rejects negatives.
[[nodiscard]] inline std::expected<std::int32_t, Error> read_le_array_length(Cursor &c) noexcept
{
	const std::size_t pos_before = c.pos;
	auto len = c.read_int_le<std::int32_t>();
	if (!len) {
		return std::unexpected(len.error());
	}
	if (*len < 0) {
		return std::unexpected(make_error(ErrorCode::NegativeLength, pos_before));
	}
	return *len;
}

// Read an array length (ZigZag VarInt for Bedrock VarInt network). Rejects negatives.
[[nodiscard]] inline std::expected<std::int32_t, Error> read_varint_count(Cursor &c)
{
	const std::size_t pos_before = c.pos;
	auto v = read_zigzag32(c);
	if (!v) {
		return std::unexpected(v.error());
	}
	if (*v < 0) {
		return std::unexpected(make_error(ErrorCode::NegativeLength, pos_before));
	}
	return *v;
}

// Skip an NBT string payload: read the dialect-specific length prefix and
// advance the cursor past the payload bytes.
[[nodiscard]] inline std::expected<void, Error> skip_be_string(Cursor &c) noexcept
{
	auto len = c.read_int_be<std::uint16_t>();
	if (!len) {
		return std::unexpected(len.error());
	}
	if (auto e = c.need(*len); !e) {
		return e;
	}
	c.skip(*len);
	return {};
}

[[nodiscard]] inline std::expected<void, Error> skip_le_string(Cursor &c) noexcept
{
	auto len = c.read_int_le<std::uint16_t>();
	if (!len) {
		return std::unexpected(len.error());
	}
	if (auto e = c.need(*len); !e) {
		return e;
	}
	c.skip(*len);
	return {};
}

[[nodiscard]] inline std::expected<void, Error> skip_varint_string(Cursor &c)
{
	auto len = read_uvarint32(c);
	if (!len) {
		return std::unexpected(len.error());
	}
	if (auto e = c.need(*len); !e) {
		return e;
	}
	c.skip(*len);
	return {};
}

} // namespace tagforge::detail
