// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Public `tagforge::skip` entry points. Selects the dialect-specific
// implementation in src/skip/skip_<format>.cpp.

#include "tagforge/skip.hpp"

#include "../internal/skip_internal.hpp"
#include "tagforge/cursor.hpp"
#include "tagforge/format.hpp"

#include <expected>

namespace tagforge {

namespace {

[[nodiscard]] std::expected<TagId, Error> dispatch(Cursor &c, Format f)
{
	switch (f) {
	case Format::JavaNamedRoot:
		return detail::skip_java_named(c);
	case Format::JavaAnonymousRoot:
		return detail::skip_java_anonymous(c);
	case Format::BedrockLittleEndian:
		return detail::skip_bedrock_le(c);
	case Format::BedrockVarInt:
		return detail::skip_bedrock_varint(c);
	}
	return std::unexpected(make_error(ErrorCode::InvalidRoot, 0, "unknown format"));
}

} // namespace

std::expected<std::size_t, Error> skip(std::span<const std::byte> bytes, Format format)
{
	Cursor c{bytes, 0};
	auto r = dispatch(c, format);
	if (!r) {
		return std::unexpected(r.error());
	}
	return c.pos;
}

std::expected<std::size_t, Error> skip_expect(std::span<const std::byte> bytes, Format format, TagId expected_root)
{
	Cursor c{bytes, 0};
	auto r = dispatch(c, format);
	if (!r) {
		return std::unexpected(r.error());
	}
	if (*r != expected_root) {
		return std::unexpected(
			make_error(ErrorCode::UnexpectedRootType, 0, "root tag did not match expected_root"));
	}
	return c.pos;
}

std::expected<std::size_t, Error> skip_auto(std::span<const std::byte> bytes)
{
	auto fmt = detect_format(bytes);
	if (!fmt) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, 0, "could not detect NBT dialect"));
	}
	return skip(bytes, *fmt);
}

} // namespace tagforge
