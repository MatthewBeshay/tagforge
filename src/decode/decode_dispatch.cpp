// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Public `tagforge::decode` entry points. Selects the dialect-specific
// implementation in src/decode/decode_<format>.cpp.

#include "tagforge/decode.hpp"

#include "../internal/decode_internal.hpp"
#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"

#include <expected>

namespace tagforge {

namespace {

[[nodiscard]] std::expected<NamedValue, Error> dispatch_named(Cursor &c, Format f)
{
	switch (f) {
	case Format::JavaNamedRoot:
		return detail::decode_java_named(c);
	case Format::JavaAnonymousRoot: {
		// For symmetry: decode anonymously and wrap with an empty name.
		auto v = detail::decode_java_anonymous(c);
		if (!v) {
			return std::unexpected(v.error());
		}
		return NamedValue{.name = {}, .value = std::move(*v)};
	}
	case Format::BedrockLittleEndian:
		return detail::decode_bedrock_le(c);
	case Format::BedrockVarInt:
		return detail::decode_bedrock_varint(c);
	}
	return std::unexpected(make_error(ErrorCode::InvalidRoot, 0, "unknown format"));
}

[[nodiscard]] std::expected<Value, Error> dispatch_anonymous(Cursor &c, Format f)
{
	switch (f) {
	case Format::JavaAnonymousRoot:
		return detail::decode_java_anonymous(c);
	case Format::JavaNamedRoot: {
		auto nv = detail::decode_java_named(c);
		if (!nv) {
			return std::unexpected(nv.error());
		}
		return std::move(nv->value);
	}
	case Format::BedrockLittleEndian: {
		auto nv = detail::decode_bedrock_le(c);
		if (!nv) {
			return std::unexpected(nv.error());
		}
		return std::move(nv->value);
	}
	case Format::BedrockVarInt: {
		auto nv = detail::decode_bedrock_varint(c);
		if (!nv) {
			return std::unexpected(nv.error());
		}
		return std::move(nv->value);
	}
	}
	return std::unexpected(make_error(ErrorCode::InvalidRoot, 0, "unknown format"));
}

} // namespace

std::expected<NamedValue, Error> decode(std::span<const std::byte> bytes, Format format)
{
	Cursor c{bytes, 0};
	return dispatch_named(c, format);
}

std::expected<Value, Error> decode_anonymous(std::span<const std::byte> bytes, Format format)
{
	Cursor c{bytes, 0};
	return dispatch_anonymous(c, format);
}

std::expected<NamedValue, Error> Decoder::decode(Cursor &c) const
{
	return dispatch_named(c, format_);
}

std::expected<Value, Error> Decoder::decode_anonymous(Cursor &c) const
{
	return dispatch_anonymous(c, format_);
}

std::expected<NamedValue, Error> decode_auto(std::span<const std::byte> bytes)
{
	auto fmt = detect_format(bytes);
	if (!fmt) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, 0, "could not detect NBT dialect"));
	}
	return decode(bytes, *fmt);
}

} // namespace tagforge
