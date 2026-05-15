// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/tag_id.hpp"

#include <cstddef>
#include <expected>
#include <span>

namespace tagforge {

// Skip a single NBT root in the given format without allocating. Returns the
// number of bytes consumed.
//
// For JavaAnonymousRoot, a single bare 0x00 byte at the start of `bytes` is
// treated as the "no NBT" sentinel used by 1.21.4 block-entity records: skip
// returns 1.
[[nodiscard]] std::expected<std::size_t, Error> skip(std::span<const std::byte> bytes, Format format);

// As skip(), but additionally asserts the root tag id matches `expected_root`.
[[nodiscard]] std::expected<std::size_t, Error> skip_expect(std::span<const std::byte> bytes, Format format,
							    TagId expected_root);

// Auto-detect the dialect (via tagforge::detect_format) and skip in one call.
[[nodiscard]] std::expected<std::size_t, Error> skip_auto(std::span<const std::byte> bytes);

} // namespace tagforge
