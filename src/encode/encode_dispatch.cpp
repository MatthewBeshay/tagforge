// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Public `tagforge::encode` entry points. Selects the dialect-specific
// implementation in src/encode/encode_<format>.cpp.

#include "tagforge/encode.hpp"

#include "../internal/encode_internal.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstring>
#include <expected>
#include <vector>

namespace tagforge {

namespace {

[[nodiscard]] std::expected<void, Error> dispatch_named(const NamedValue &nv, Format f, std::vector<std::byte> &out)
{
	switch (f) {
	case Format::JavaNamedRoot:
		return detail::encode_java_named(nv, out);
	case Format::JavaAnonymousRoot:
		return detail::encode_java_anonymous(nv.value, out);
	case Format::BedrockLittleEndian:
		return detail::encode_bedrock_le(nv, out);
	case Format::BedrockVarInt:
		return detail::encode_bedrock_varint(nv, out);
	}
	return std::unexpected(make_error(ErrorCode::InvalidRoot, 0, "unknown format"));
}

[[nodiscard]] std::expected<void, Error> dispatch_anonymous(const Value &v, Format f, std::vector<std::byte> &out)
{
	switch (f) {
	case Format::JavaAnonymousRoot:
		return detail::encode_java_anonymous(v, out);
	case Format::JavaNamedRoot:
		return detail::encode_java_named(NamedValue{.name = {}, .value = v}, out);
	case Format::BedrockLittleEndian:
		return detail::encode_bedrock_le(NamedValue{.name = {}, .value = v}, out);
	case Format::BedrockVarInt:
		return detail::encode_bedrock_varint(NamedValue{.name = {}, .value = v}, out);
	}
	return std::unexpected(make_error(ErrorCode::InvalidRoot, 0, "unknown format"));
}

} // namespace

std::expected<std::vector<std::byte>, Error> encode(const NamedValue &nv, Format format)
{
	std::vector<std::byte> out;
	if (auto r = dispatch_named(nv, format, out); !r) {
		return std::unexpected(r.error());
	}
	return out;
}

std::expected<std::vector<std::byte>, Error> encode_anonymous(const Value &v, Format format)
{
	std::vector<std::byte> out;
	if (auto r = dispatch_anonymous(v, format, out); !r) {
		return std::unexpected(r.error());
	}
	return out;
}

std::expected<std::size_t, Error> encode_into(std::span<std::byte> out, const NamedValue &nv, Format format)
{
	std::vector<std::byte> buf;
	if (auto r = dispatch_named(nv, format, buf); !r) {
		return std::unexpected(r.error());
	}
	if (buf.size() > out.size()) {
		return std::unexpected(make_error(ErrorCode::LengthOverflow, 0, "destination too small"));
	}
	std::memcpy(out.data(), buf.data(), buf.size());
	return buf.size();
}

std::expected<std::size_t, Error> encode_anonymous_into(std::span<std::byte> out, const Value &v, Format format)
{
	std::vector<std::byte> buf;
	if (auto r = dispatch_anonymous(v, format, buf); !r) {
		return std::unexpected(r.error());
	}
	if (buf.size() > out.size()) {
		return std::unexpected(make_error(ErrorCode::LengthOverflow, 0, "destination too small"));
	}
	std::memcpy(out.data(), buf.data(), buf.size());
	return buf.size();
}

std::expected<void, Error> Encoder::write(const NamedValue &nv)
{
	return dispatch_named(nv, format_, *sink_);
}

std::expected<void, Error> Encoder::write_anonymous(const Value &v)
{
	return dispatch_anonymous(v, format_, *sink_);
}

} // namespace tagforge
