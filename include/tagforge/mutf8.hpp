// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace tagforge {

// Modified-UTF-8 (the Java/NBT flavour) ↔ standard UTF-8 transcoders.
//
// MUTF-8 differs from standard UTF-8 in two ways:
//
//   * U+0000 is encoded as the 2-byte sequence 0xC0 0x80 (rather than 0x00),
//     so MUTF-8 byte runs can be C-string safe.
//   * Supplementary code points (U+10000..U+10FFFF) are encoded as a UTF-16
//     surrogate pair, each half then re-encoded as a 3-byte MUTF-8 sequence
//     - six bytes total - rather than the 4-byte UTF-8 form.
//
// Both transcoders are strict: any input that does not match the encoding
// rules above is rejected with ErrorCode::InvalidMutf8 or InvalidUtf8 and a
// byte offset pointing at the first offending byte.

[[nodiscard]] std::expected<std::string, Error> mutf8_to_utf8(std::span<const std::byte> bytes);
[[nodiscard]] std::expected<std::string, Error> mutf8_to_utf8(std::string_view bytes);

[[nodiscard]] std::expected<std::string, Error> utf8_to_mutf8(std::string_view utf8);

// Fast path: returns true when the byte run contains only 7-bit ASCII, in
// which case MUTF-8 and UTF-8 byte streams are bit-identical and no
// transcoding is needed.
[[nodiscard]] bool is_pure_ascii(std::span<const std::byte>) noexcept;
[[nodiscard]] bool is_pure_ascii(std::string_view) noexcept;

} // namespace tagforge
