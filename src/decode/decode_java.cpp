// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "../internal/decode_internal.hpp"
#include "../internal/limits.hpp"
#include "../internal/name_io.hpp"
#include "../internal/wire_helpers.hpp"

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"

#include <cstdint>
#include <cstring>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace tagforge::detail {

namespace {

[[nodiscard]] std::expected<Value, Error> decode_payload(Cursor &c, TagId tag, int depth);

[[nodiscard]] std::expected<Value, Error> decode_string_payload(Cursor &c)
{
	auto s = read_string_be_mutf8(c);
	if (!s) {
		return std::unexpected(s.error());
	}
	return Value{.v = std::move(*s)};
}

[[nodiscard]] std::expected<Value, Error> decode_byte_array(Cursor &c)
{
	auto len = read_be_array_length(c);
	if (!len) {
		return std::unexpected(len.error());
	}
	std::vector<std::int8_t> data(static_cast<std::size_t>(*len));
	if (auto e = c.need(data.size()); !e) {
		return std::unexpected(e.error());
	}
	std::memcpy(data.data(), c.data.data() + c.pos, data.size());
	c.skip(data.size());
	return Value{.v = std::move(data)};
}

[[nodiscard]] std::expected<Value, Error> decode_int_array(Cursor &c)
{
	auto len = read_be_array_length(c);
	if (!len) {
		return std::unexpected(len.error());
	}
	std::vector<std::int32_t> data;
	data.reserve(static_cast<std::size_t>(*len));
	for (std::int32_t i = 0; i < *len; ++i) {
		auto v = c.read_int_be<std::int32_t>();
		if (!v) {
			return std::unexpected(v.error());
		}
		data.push_back(*v);
	}
	return Value{.v = std::move(data)};
}

[[nodiscard]] std::expected<Value, Error> decode_long_array(Cursor &c)
{
	auto len = read_be_array_length(c);
	if (!len) {
		return std::unexpected(len.error());
	}
	std::vector<std::int64_t> data;
	data.reserve(static_cast<std::size_t>(*len));
	for (std::int32_t i = 0; i < *len; ++i) {
		auto v = c.read_int_be<std::int64_t>();
		if (!v) {
			return std::unexpected(v.error());
		}
		data.push_back(*v);
	}
	return Value{.v = std::move(data)};
}

[[nodiscard]] std::expected<Value, Error> decode_list(Cursor &c, int depth)
{
	auto child = read_tag_byte(c);
	if (!child) {
		return std::unexpected(child.error());
	}
	auto count = read_be_array_length(c);
	if (!count) {
		return std::unexpected(count.error());
	}
	List list;
	// List<End, N>: the wire's degenerate "empty list of unknown type" form.
	// We collapse this to an empty list; on encode it becomes List<End, 0>.
	if (*child == TagId::End) {
		return Value{.v = std::move(list)};
	}
	list.reserve(static_cast<std::size_t>(*count));
	for (std::int32_t i = 0; i < *count; ++i) {
		auto elem = decode_payload(c, *child, depth);
		if (!elem) {
			return std::unexpected(elem.error());
		}
		list.push_back(std::move(*elem));
	}
	return Value{.v = std::move(list)};
}

[[nodiscard]] std::expected<Value, Error> decode_compound(Cursor &c, int depth)
{
	Compound compound;
	while (true) {
		auto child = read_tag_byte(c);
		if (!child) {
			return std::unexpected(child.error());
		}
		if (*child == TagId::End) {
			return Value{.v = std::move(compound)};
		}
		auto name = read_string_be_mutf8(c);
		if (!name) {
			return std::unexpected(name.error());
		}
		auto payload = decode_payload(c, *child, depth);
		if (!payload) {
			return std::unexpected(payload.error());
		}
		compound.emplace_back(std::move(*name), std::move(*payload));
	}
}

[[nodiscard]] std::expected<Value, Error> decode_payload(Cursor &c, TagId tag, int depth)
{
	if (depth >= kMaxDecodeDepth) {
		return std::unexpected(depth_exceeded(c.pos));
	}
	switch (tag) {
	case TagId::End:
		return Value{.v = EndTag{}};
	case TagId::Byte: {
		auto v = c.read_int_be<std::int8_t>();
		if (!v) {
			return std::unexpected(v.error());
		}
		return Value{.v = *v};
	}
	case TagId::Short: {
		auto v = c.read_int_be<std::int16_t>();
		if (!v) {
			return std::unexpected(v.error());
		}
		return Value{.v = *v};
	}
	case TagId::Int: {
		auto v = c.read_int_be<std::int32_t>();
		if (!v) {
			return std::unexpected(v.error());
		}
		return Value{.v = *v};
	}
	case TagId::Long: {
		auto v = c.read_int_be<std::int64_t>();
		if (!v) {
			return std::unexpected(v.error());
		}
		return Value{.v = *v};
	}
	case TagId::Float: {
		auto v = c.read_float_be();
		if (!v) {
			return std::unexpected(v.error());
		}
		return Value{.v = *v};
	}
	case TagId::Double: {
		auto v = c.read_double_be();
		if (!v) {
			return std::unexpected(v.error());
		}
		return Value{.v = *v};
	}
	case TagId::ByteArray:
		return decode_byte_array(c);
	case TagId::String:
		return decode_string_payload(c);
	case TagId::List:
		return decode_list(c, depth + 1);
	case TagId::Compound:
		return decode_compound(c, depth + 1);
	case TagId::IntArray:
		return decode_int_array(c);
	case TagId::LongArray:
		return decode_long_array(c);
	}
	return std::unexpected(make_error(ErrorCode::UnknownTagId, c.pos));
}

} // namespace

std::expected<NamedValue, Error> decode_java_named(Cursor &c)
{
	auto tag = read_tag_byte(c);
	if (!tag) {
		return std::unexpected(tag.error());
	}
	if (*tag == TagId::End) {
		return NamedValue{.name = {}, .value = Value{.v = EndTag{}}};
	}
	auto name = read_string_be_mutf8(c);
	if (!name) {
		return std::unexpected(name.error());
	}
	auto payload = decode_payload(c, *tag, 0);
	if (!payload) {
		return std::unexpected(payload.error());
	}
	return NamedValue{.name = std::move(*name), .value = std::move(*payload)};
}

std::expected<Value, Error> decode_java_anonymous(Cursor &c)
{
	auto tag = read_tag_byte(c);
	if (!tag) {
		return std::unexpected(tag.error());
	}
	if (*tag == TagId::End) {
		return Value{.v = EndTag{}};
	}
	return decode_payload(c, *tag, 0);
}

} // namespace tagforge::detail
