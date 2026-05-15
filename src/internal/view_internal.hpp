// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Per-dialect helpers used by the zero-copy View to walk a compound or
// list without materialising children. Each pair of functions answers
// "where does the payload at offset X end?" (payload_size) and "what's
// at offset X, and where does it continue?" (read_child).

#pragma once

#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/tag_id.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <string_view>

namespace tagforge::detail {

// Result of inspecting a compound's child entry without materialising it.
struct ChildRecord {
	TagId tag;                  // child tag
	std::size_t name_offset;    // offset to the first name byte
	std::size_t name_length;    // wire-MUTF-8 byte length
	std::size_t payload_offset; // start of the payload
	std::size_t payload_size;   // number of payload bytes
};

// Total wire size of one named child entry = (payload_offset - tag_offset) + payload_size.
[[nodiscard]] inline std::size_t child_total_size(const ChildRecord &cr, std::size_t tag_offset) noexcept
{
	return (cr.payload_offset - tag_offset) + cr.payload_size;
}

// View-time helpers per dialect. Each takes the buffer + an offset and either
// inspects the current child entry (read tag, name, find payload size) or
// computes the size of a payload of a given tag at the cursor.

[[nodiscard]] std::expected<std::size_t, Error> java_payload_size(std::span<const std::byte> bytes, std::size_t offset,
								  TagId tag);

[[nodiscard]] std::expected<ChildRecord, Error> java_read_child(std::span<const std::byte> bytes, std::size_t offset);

[[nodiscard]] std::expected<std::size_t, Error> bedrock_le_payload_size(std::span<const std::byte> bytes,
									std::size_t offset, TagId tag);

[[nodiscard]] std::expected<ChildRecord, Error> bedrock_le_read_child(std::span<const std::byte> bytes,
								      std::size_t offset);

[[nodiscard]] std::expected<std::size_t, Error> bedrock_varint_payload_size(std::span<const std::byte> bytes,
									    std::size_t offset, TagId tag);

[[nodiscard]] std::expected<ChildRecord, Error> bedrock_varint_read_child(std::span<const std::byte> bytes,
									  std::size_t offset);

[[nodiscard]] std::expected<std::size_t, Error> view_payload_size(Format f, std::span<const std::byte> bytes,
								  std::size_t offset, TagId tag);

[[nodiscard]] std::expected<ChildRecord, Error> view_read_child(Format f, std::span<const std::byte> bytes,
								std::size_t offset);

} // namespace tagforge::detail
