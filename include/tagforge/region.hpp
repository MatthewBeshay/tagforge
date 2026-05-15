// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/compress.hpp"
#include "tagforge/error.hpp"
#include "tagforge/value.hpp"
#include "tagforge/view.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace tagforge {

// Reader + writer for Minecraft Java region files (.mca / .mcr).
//
// A region file holds an 8 KiB header (1024 location entries + 1024 modification
// timestamps) followed by per-chunk payloads laid out in 4 KiB sectors. Each
// chunk is independently compressed (gzip, zlib, or raw) with named-root NBT.
class Region {
public:
	// Local chunk coordinates inside the region are 0..31 on both axes.
	static constexpr int kSize = 32;

	// Empty region: 8 KiB header, no chunks. Suitable as a starting point
	// for building a region file from scratch.
	[[nodiscard]] static Region create() noexcept;

	[[nodiscard]] static std::expected<Region, Error> open(const std::filesystem::path &);
	[[nodiscard]] static std::expected<Region, Error> open_from_bytes(std::vector<std::byte> bytes);

	[[nodiscard]] bool has_chunk(int cx, int cz) const noexcept;
	[[nodiscard]] std::uint32_t chunk_timestamp(int cx, int cz) const noexcept;

	// Returns the *decompressed* chunk payload (named-root NBT bytes).
	[[nodiscard]] std::expected<std::vector<std::byte>, Error> chunk_raw(int cx, int cz);

	[[nodiscard]] std::expected<NamedValue, Error> chunk_value(int cx, int cz);
	[[nodiscard]] std::expected<View, Error> chunk_view(int cx, int cz);

	// Enumerate the (cx, cz) coordinates of every populated chunk slot,
	// row-major (cz major, cx minor). Order does not reflect the file's
	// on-disk sector layout. Returned coordinates are in [0, kSize).
	[[nodiscard]] std::vector<std::pair<int, int>> populated_chunks() const;

	// Encode `nv` as Java named-root NBT, compress with `codec`, and
	// install it at (cx, cz). Existing data at (cx, cz) is replaced. The
	// sector layout is rewritten lazily on `save_to_bytes` / `save`.
	[[nodiscard]] std::expected<void, Error> write_chunk(int cx, int cz, const NamedValue &nv,
							     Codec codec = Codec::Zlib,
							     std::uint32_t timestamp_unix = 0,
							     int compression_level = compression_level::balanced);

	// Replace a chunk with already-encoded named-root NBT bytes (will be
	// compressed using `codec`). Convenient when you have pre-built bytes
	// from `tagforge::encode`.
	[[nodiscard]] std::expected<void, Error>
	write_chunk_bytes(int cx, int cz, std::span<const std::byte> named_root_nbt, Codec codec = Codec::Zlib,
			  std::uint32_t timestamp_unix = 0, int compression_level = compression_level::balanced);

	// Remove a chunk slot. The next save will leave the location entry zeroed.
	void remove_chunk(int cx, int cz) noexcept;

	// Serialise the region back to bytes, with chunks packed in row-major
	// order starting at sector 2. The result is a complete `.mca` payload.
	[[nodiscard]] std::expected<std::vector<std::byte>, Error> save_to_bytes() const;

	// Convenience wrapper around save_to_bytes + file write.
	[[nodiscard]] std::expected<void, Error> save(const std::filesystem::path &) const;

private:
	Region() = default;

	struct ChunkEntry {
		std::vector<std::byte> compressed; // includes 5-byte chunk header
		std::uint32_t timestamp = 0;
		Codec codec = Codec::None;
		bool present = false;
	};

	// For the read path: the original file bytes are kept so existing
	// chunk_raw / chunk_view calls keep working against an mmap-style view.
	// Once write_chunk is called for a slot, the in-memory ChunkEntry takes
	// priority on read.
	std::vector<std::byte> bytes_;

	// Decompressed payload cache, keyed by chunk index. Populated on
	// first read; invalidated when a slot is written.
	std::vector<std::vector<std::byte>> cache_;

	// Per-slot pending writes / removes. `present == false` means "no
	// override"; `present == true && compressed.empty()` means "remove".
	std::vector<ChunkEntry> overrides_;
};

} // namespace tagforge
