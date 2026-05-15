// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "tagforge/compress.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tagforge {

std::optional<Codec> detect_codec(std::span<const std::byte> bytes) noexcept
{
	// LZ4 Frame magic 0x184D2204 stored little-endian.
	if (bytes.size() >= 4) {
		const auto b0 = static_cast<std::uint8_t>(bytes[0]);
		const auto b1 = static_cast<std::uint8_t>(bytes[1]);
		const auto b2 = static_cast<std::uint8_t>(bytes[2]);
		const auto b3 = static_cast<std::uint8_t>(bytes[3]);
		if (b0 == 0x04 && b1 == 0x22 && b2 == 0x4D && b3 == 0x18) {
			return Codec::Lz4;
		}
	}

	if (bytes.size() < 2) {
		return std::nullopt;
	}
	const auto b0 = static_cast<std::uint8_t>(bytes[0]);
	const auto b1 = static_cast<std::uint8_t>(bytes[1]);

	if (b0 == 0x1F && b1 == 0x8B) {
		return Codec::Gzip;
	}
	// zlib: high nibble of b0 == 8 (deflate), low nibble == log2(window/2) - 1 (typically 7).
	// The (b0 * 256 + b1) checksum must be divisible by 31.
	if ((b0 & 0x0F) == 0x08 && (b0 >> 4) <= 7 && ((static_cast<unsigned>(b0) << 8) + b1) % 31u == 0u) {
		return Codec::Zlib;
	}
	return std::nullopt;
}

} // namespace tagforge
