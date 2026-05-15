// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "../internal/view_internal.hpp"
#include "../internal/wire_helpers.hpp"

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"

#include <cstdint>
#include <expected>
#include <span>

namespace tagforge::detail {

namespace {

[[nodiscard]] std::expected<std::size_t, Error> skip_payload(Cursor &c, TagId tag);

[[nodiscard]] std::expected<std::size_t, Error> skip_string_sized(Cursor &c)
{
	const std::size_t before = c.pos;
	if (auto r = skip_le_string(c); !r) {
		return std::unexpected(r.error());
	}
	return c.pos - before;
}

[[nodiscard]] std::expected<std::size_t, Error> skip_payload(Cursor &c, TagId tag)
{
	const std::size_t start = c.pos;
	switch (tag) {
	case TagId::End:
		return std::size_t{0};
	case TagId::Byte:
		if (auto e = c.need(1); !e) {
			return std::unexpected(e.error());
		}
		c.skip(1);
		return c.pos - start;
	case TagId::Short:
		if (auto e = c.need(2); !e) {
			return std::unexpected(e.error());
		}
		c.skip(2);
		return c.pos - start;
	case TagId::Int:
	case TagId::Float:
		if (auto e = c.need(4); !e) {
			return std::unexpected(e.error());
		}
		c.skip(4);
		return c.pos - start;
	case TagId::Long:
	case TagId::Double:
		if (auto e = c.need(8); !e) {
			return std::unexpected(e.error());
		}
		c.skip(8);
		return c.pos - start;
	case TagId::ByteArray: {
		auto len = read_le_array_length(c);
		if (!len) {
			return std::unexpected(len.error());
		}
		if (auto e = c.need(*len); !e) {
			return std::unexpected(e.error());
		}
		c.skip(*len);
		return c.pos - start;
	}
	case TagId::String:
		if (auto r = skip_string_sized(c); !r) {
			return r;
		}
		return c.pos - start;
	case TagId::List: {
		auto child = read_tag_byte(c);
		if (!child) {
			return std::unexpected(child.error());
		}
		auto count = read_le_array_length(c);
		if (!count) {
			return std::unexpected(count.error());
		}
		if (*child == TagId::End) {
			return c.pos - start;
		}
		for (std::int32_t i = 0; i < *count; ++i) {
			auto r = skip_payload(c, *child);
			if (!r) {
				return r;
			}
		}
		return c.pos - start;
	}
	case TagId::Compound: {
		while (true) {
			auto child = read_tag_byte(c);
			if (!child) {
				return std::unexpected(child.error());
			}
			if (*child == TagId::End) {
				return c.pos - start;
			}
			if (auto r = skip_string_sized(c); !r) {
				return std::unexpected(r.error());
			}
			auto r = skip_payload(c, *child);
			if (!r) {
				return r;
			}
		}
	}
	case TagId::IntArray: {
		auto len = read_le_array_length(c);
		if (!len) {
			return std::unexpected(len.error());
		}
		const std::size_t bytes = static_cast<std::size_t>(*len) * 4;
		if (auto e = c.need(bytes); !e) {
			return std::unexpected(e.error());
		}
		c.skip(bytes);
		return c.pos - start;
	}
	case TagId::LongArray: {
		auto len = read_le_array_length(c);
		if (!len) {
			return std::unexpected(len.error());
		}
		const std::size_t bytes = static_cast<std::size_t>(*len) * 8;
		if (auto e = c.need(bytes); !e) {
			return std::unexpected(e.error());
		}
		c.skip(bytes);
		return c.pos - start;
	}
	}
	return std::unexpected(make_error(ErrorCode::UnknownTagId, c.pos));
}

} // namespace

std::expected<std::size_t, Error> bedrock_le_payload_size(std::span<const std::byte> bytes, std::size_t offset,
							  TagId tag)
{
	Cursor c{bytes, offset};
	return skip_payload(c, tag);
}

std::expected<ChildRecord, Error> bedrock_le_read_child(std::span<const std::byte> bytes, std::size_t offset)
{
	Cursor c{bytes, offset};
	auto tag = read_tag_byte(c);
	if (!tag) {
		return std::unexpected(tag.error());
	}
	if (*tag == TagId::End) {
		return ChildRecord{.tag = TagId::End,
				   .name_offset = c.pos,
				   .name_length = 0,
				   .payload_offset = c.pos,
				   .payload_size = 0};
	}
	auto name_len = c.read_int_le<std::uint16_t>();
	if (!name_len) {
		return std::unexpected(name_len.error());
	}
	if (auto e = c.need(*name_len); !e) {
		return std::unexpected(e.error());
	}
	const std::size_t name_off = c.pos;
	c.skip(*name_len);
	const std::size_t payload_off = c.pos;
	auto size = skip_payload(c, *tag);
	if (!size) {
		return std::unexpected(size.error());
	}
	return ChildRecord{.tag = *tag,
			   .name_offset = name_off,
			   .name_length = *name_len,
			   .payload_offset = payload_off,
			   .payload_size = *size};
}

} // namespace tagforge::detail
