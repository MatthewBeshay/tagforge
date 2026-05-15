// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Internal - per-dialect skip entry points. Used by src/skip/skip_dispatch.cpp.

#pragma once

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"

#include <expected>

namespace tagforge::detail {

// Java big-endian, named root: type byte, uint16 BE nameLen, name, payload.
// A bare TAG_End (0x00) is accepted as the "no NBT" sentinel.
[[nodiscard]] std::expected<TagId, Error> skip_java_named(Cursor &c);

// Java big-endian, anonymous root: type byte, payload (no name).
// A bare TAG_End (0x00) is the "no NBT" sentinel.
[[nodiscard]] std::expected<TagId, Error> skip_java_anonymous(Cursor &c);

// Bedrock little-endian, named root.
[[nodiscard]] std::expected<TagId, Error> skip_bedrock_le(Cursor &c);

// Bedrock varint network: little-endian primitives, ZigZag VarInt integers
// (string lengths, list counts, array lengths). Named root.
[[nodiscard]] std::expected<TagId, Error> skip_bedrock_varint(Cursor &c);

} // namespace tagforge::detail
