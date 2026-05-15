// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace tagforge {

enum class Codec : std::uint8_t {
	None = 0,
	Gzip = 1,
	Zlib = 2,
	// LZ4 Frame format (the de-facto standard wrapper for `.lz4` files and
	// some 1.20.5+ Minecraft region chunks). The 4-byte magic is
	// 0x184D2204 (stored little-endian - file begins with 04 22 4D 18).
	Lz4 = 3,
};

// libdeflate compression levels. Higher = smaller output, slower. 1 is the
// fastest practical setting, 12 is libdeflate's slowest/best.
//
// LZ4 uses a different scale (1..12 also, where 1 is fastest, 12 = HC max);
// these names map sensibly onto both libraries.
namespace compression_level {
inline constexpr int fastest = 1;
inline constexpr int fast = 3;
inline constexpr int balanced = 6; // libdeflate's default
inline constexpr int high = 9;     // matches zlib's "best"
inline constexpr int max = 12;     // ceiling for both libdeflate and lz4-hc
} // namespace compression_level

struct DecompressOptions {
	// Hard upper bound on output size; protects against zip-bomb inputs.
	// Default 64 MiB. Set higher when reading region files with very
	// large chunks.
	std::size_t max_output = 64ull * 1024 * 1024;
};

// Detect the codec by magic bytes:
//   - gzip:   0x1F 0x8B
//   - zlib:   0x78 followed by one of {0x01, 0x5E, 0x9C, 0xDA}
//   - LZ4:    0x04 0x22 0x4D 0x18 (LZ4 Frame magic, little-endian)
//   - else:   nullopt (caller must decide; typically Codec::None == raw bytes)
[[nodiscard]] std::optional<Codec> detect_codec(std::span<const std::byte> bytes) noexcept;

// Decompress with codec auto-detection. If no gzip/zlib magic is recognised,
// returns the input bytes as a copy (codec == None).
[[nodiscard]] std::expected<std::vector<std::byte>, Error> decompress(std::span<const std::byte> bytes,
								      const DecompressOptions &opts = {});

[[nodiscard]] std::expected<std::vector<std::byte>, Error> decompress(std::span<const std::byte> bytes, Codec codec,
								      const DecompressOptions &opts = {});

[[nodiscard]] std::expected<std::vector<std::byte>, Error> compress(std::span<const std::byte> bytes, Codec codec,
								    int level = compression_level::balanced);

} // namespace tagforge
