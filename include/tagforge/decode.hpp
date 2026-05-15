// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <expected>
#include <span>

namespace tagforge {

// Decode a single NBT root in the given format into an owning tree.
//
// `decode_anonymous` accepts the bare-0x00 "no NBT" sentinel and returns a
// Value holding EndTag{} (i.e. kind() == TagId::End). Callers can treat the
// EndTag arm as "the wire carried no payload".
[[nodiscard]] std::expected<NamedValue, Error> decode(std::span<const std::byte> bytes, Format format);
[[nodiscard]] std::expected<Value, Error> decode_anonymous(std::span<const std::byte> bytes, Format format);

// Convenience: auto-detect the dialect via tagforge::detect_format and decode
// in one call. Returns UnexpectedRootType when the detector cannot identify a
// format. For network NBT (anonymous root) the caller must call
// decode_anonymous with an explicit format - the detector cannot distinguish
// anonymous-root from named-root for non-zero leading bytes.
[[nodiscard]] std::expected<NamedValue, Error> decode_auto(std::span<const std::byte> bytes);

// Streaming decoder. Useful for callers who own a Cursor over a larger buffer
// (e.g. an NBT field embedded inside a packet) and want to advance the cursor
// after the decode.
class Decoder {
public:
	explicit Decoder(Format f) noexcept : format_{f} {}

	[[nodiscard]] std::expected<NamedValue, Error> decode(Cursor &c) const;
	[[nodiscard]] std::expected<Value, Error> decode_anonymous(Cursor &c) const;

private:
	Format format_;
};

} // namespace tagforge
