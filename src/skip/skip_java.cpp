// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "../internal/limits.hpp"
#include "../internal/skip_internal.hpp"
#include "../internal/wire_helpers.hpp"

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"

#include <cstdint>
#include <expected>

namespace tagforge::detail {

namespace {

[[nodiscard]] std::expected<void, Error> skip_payload_java_be(Cursor &c, TagId tag, int depth)
{
	if (depth >= kMaxDecodeDepth) {
		return std::unexpected(depth_exceeded(c.pos));
	}
	switch (tag) {
	case TagId::End:
		return {};
	case TagId::Byte:
		if (auto e = c.need(1); !e) {
			return e;
		}
		c.skip(1);
		return {};
	case TagId::Short:
		if (auto e = c.need(2); !e) {
			return e;
		}
		c.skip(2);
		return {};
	case TagId::Int:
	case TagId::Float:
		if (auto e = c.need(4); !e) {
			return e;
		}
		c.skip(4);
		return {};
	case TagId::Long:
	case TagId::Double:
		if (auto e = c.need(8); !e) {
			return e;
		}
		c.skip(8);
		return {};
	case TagId::ByteArray: {
		auto len = read_be_array_length(c);
		if (!len) {
			return std::unexpected(len.error());
		}
		const std::size_t bytes = static_cast<std::size_t>(*len);
		if (auto e = c.need(bytes); !e) {
			return e;
		}
		c.skip(bytes);
		return {};
	}
	case TagId::String:
		return skip_be_string(c);
	case TagId::List: {
		auto child = read_tag_byte(c);
		if (!child) {
			return std::unexpected(child.error());
		}
		auto count = read_be_array_length(c);
		if (!count) {
			return std::unexpected(count.error());
		}
		// List<End>: degenerate "list of unknown type"; no per-element payload.
		if (*child == TagId::End) {
			return {};
		}
		for (std::int32_t i = 0; i < *count; ++i) {
			auto r = skip_payload_java_be(c, *child, depth + 1);
			if (!r) {
				return r;
			}
		}
		return {};
	}
	case TagId::Compound: {
		while (true) {
			auto child = read_tag_byte(c);
			if (!child) {
				return std::unexpected(child.error());
			}
			if (*child == TagId::End) {
				return {};
			}
			if (auto r = skip_be_string(c); !r) {
				return r;
			}
			if (auto r = skip_payload_java_be(c, *child, depth + 1); !r) {
				return r;
			}
		}
	}
	case TagId::IntArray: {
		auto len = read_be_array_length(c);
		if (!len) {
			return std::unexpected(len.error());
		}
		const std::size_t bytes = static_cast<std::size_t>(*len) * 4;
		if (auto e = c.need(bytes); !e) {
			return e;
		}
		c.skip(bytes);
		return {};
	}
	case TagId::LongArray: {
		auto len = read_be_array_length(c);
		if (!len) {
			return std::unexpected(len.error());
		}
		const std::size_t bytes = static_cast<std::size_t>(*len) * 8;
		if (auto e = c.need(bytes); !e) {
			return e;
		}
		c.skip(bytes);
		return {};
	}
	}
	return std::unexpected(make_error(ErrorCode::UnknownTagId, c.pos));
}

} // namespace

std::expected<TagId, Error> skip_java_named(Cursor &c)
{
	auto tag = read_tag_byte(c);
	if (!tag) {
		return std::unexpected(tag.error());
	}
	if (*tag == TagId::End) {
		return TagId::End;
	}
	if (auto r = skip_be_string(c); !r) {
		return std::unexpected(r.error());
	}
	if (auto r = skip_payload_java_be(c, *tag, 0); !r) {
		return std::unexpected(r.error());
	}
	return *tag;
}

std::expected<TagId, Error> skip_java_anonymous(Cursor &c)
{
	auto tag = read_tag_byte(c);
	if (!tag) {
		return std::unexpected(tag.error());
	}
	if (*tag == TagId::End) {
		return TagId::End;
	}
	if (auto r = skip_payload_java_be(c, *tag, 0); !r) {
		return std::unexpected(r.error());
	}
	return *tag;
}

} // namespace tagforge::detail
