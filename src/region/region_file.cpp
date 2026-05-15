// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Implementation of Minecraft Java region (.mca / .mcr) read + write.
// A region file is an 8 KiB header (1024 location entries + 1024
// modification timestamps, both stored as int32 BE) followed by chunk
// payloads packed into 4 KiB sectors. The sector counts in this file
// reflect that layout; the magic numbers below come from the format
// rather than implementation choices.

#include "tagforge/region.hpp"

#include "tagforge/compress.hpp"
#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/io.hpp"
#include "tagforge/value.hpp"
#include "tagforge/view.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace tagforge {

namespace {

constexpr std::size_t kSector = 4096;
constexpr std::size_t kHeaderBytes = 8192;
constexpr std::size_t kChunksPerRegion = 1024;

[[nodiscard]] std::size_t chunk_index(int cx, int cz) noexcept
{
	return static_cast<std::size_t>((cx & 31) + (cz & 31) * Region::kSize);
}

[[nodiscard]] std::uint32_t read_u32_be(const std::byte *p) noexcept
{
	std::uint32_t v = 0;
	std::memcpy(&v, p, 4);
	if (std::endian::native == std::endian::little) {
		v = std::byteswap(v);
	}
	return v;
}

void write_u32_be(std::byte *p, std::uint32_t v) noexcept
{
	if (std::endian::native == std::endian::little) {
		v = std::byteswap(v);
	}
	std::memcpy(p, &v, 4);
}

struct ChunkLocation {
	std::uint32_t sector_offset; // in 4 KiB sectors
	std::uint8_t sector_count;   // in 4 KiB sectors
};

[[nodiscard]] ChunkLocation read_location(const std::vector<std::byte> &bytes, std::size_t idx) noexcept
{
	const std::byte *p = bytes.data() + idx * 4;
	const auto raw = read_u32_be(p);
	return ChunkLocation{
		.sector_offset = raw >> 8,
		.sector_count = static_cast<std::uint8_t>(raw & 0xFFu),
	};
}

[[nodiscard]] std::uint8_t codec_to_byte(Codec c) noexcept
{
	switch (c) {
	case Codec::Gzip:
		return 1;
	case Codec::Zlib:
		return 2;
	case Codec::None:
		return 3;
	case Codec::Lz4:
		return 4; // Vanilla 1.20.5+ region chunk codec byte
	}
	return 3;
}

[[nodiscard]] std::expected<std::vector<std::byte>, Error> make_chunk_block(std::span<const std::byte> named_root_nbt,
									    Codec codec, int level)
{
	std::vector<std::byte> payload;
	if (codec == Codec::None) {
		payload.assign(named_root_nbt.begin(), named_root_nbt.end());
	} else {
		auto compressed = tagforge::compress(named_root_nbt, codec, level);
		if (!compressed) {
			return std::unexpected(compressed.error());
		}
		payload = std::move(*compressed);
	}
	// The chunk-length field is a uint32 BE that covers payload + the
	// single codec byte. Ensure the addition cannot wrap.
	if (payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max() - 1)) {
		return std::unexpected(
			make_error(ErrorCode::LengthOverflow, 0, "compressed chunk exceeds 32-bit length field"));
	}
	const std::uint32_t len_field = static_cast<std::uint32_t>(payload.size() + 1);

	std::vector<std::byte> block(5 + payload.size());
	write_u32_be(block.data(), len_field);
	block[4] = std::byte{codec_to_byte(codec)};
	std::memcpy(block.data() + 5, payload.data(), payload.size());
	return block;
}

} // namespace

Region Region::create() noexcept
{
	Region r;
	r.bytes_.assign(kHeaderBytes, std::byte{0});
	r.cache_.assign(kChunksPerRegion, std::vector<std::byte>{});
	r.overrides_.assign(kChunksPerRegion, ChunkEntry{});
	return r;
}

std::expected<Region, Error> Region::open(const std::filesystem::path &path)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f.is_open()) {
		return std::unexpected(make_error(ErrorCode::Io, 0, "could not open region file"));
	}
	const std::streamsize n = f.tellg();
	if (n < 0) {
		return std::unexpected(make_error(ErrorCode::Io));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(n));
	f.seekg(0);
	f.read(reinterpret_cast<char *>(bytes.data()), n);
	if (!f && !f.eof()) {
		return std::unexpected(make_error(ErrorCode::Io));
	}
	return open_from_bytes(std::move(bytes));
}

std::expected<Region, Error> Region::open_from_bytes(std::vector<std::byte> bytes)
{
	if (bytes.size() < kHeaderBytes) {
		return std::unexpected(
			make_error(ErrorCode::RegionInvalidHeader, bytes.size(), "file smaller than 8 KiB header"));
	}
	Region r;
	r.bytes_ = std::move(bytes);
	r.cache_.assign(kChunksPerRegion, std::vector<std::byte>{});
	r.overrides_.assign(kChunksPerRegion, ChunkEntry{});
	return r;
}

bool Region::has_chunk(int cx, int cz) const noexcept
{
	const auto idx = chunk_index(cx, cz);
	if (idx < overrides_.size() && overrides_[idx].present) {
		return !overrides_[idx].compressed.empty();
	}
	const auto loc = read_location(bytes_, idx);
	return loc.sector_offset >= 2 && loc.sector_count > 0;
}

std::uint32_t Region::chunk_timestamp(int cx, int cz) const noexcept
{
	const auto idx = chunk_index(cx, cz);
	if (idx < overrides_.size() && overrides_[idx].present) {
		return overrides_[idx].timestamp;
	}
	return read_u32_be(bytes_.data() + kSector + idx * 4);
}

std::expected<std::vector<std::byte>, Error> Region::chunk_raw(int cx, int cz)
{
	const auto idx = chunk_index(cx, cz);

	// Overrides win over the original sectors.
	if (idx < overrides_.size() && overrides_[idx].present) {
		if (overrides_[idx].compressed.empty()) {
			return std::unexpected(make_error(ErrorCode::RegionInvalidChunk, idx, "chunk removed"));
		}
		// Decompress on demand and cache.
		if (cache_[idx].empty()) {
			const auto &block = overrides_[idx].compressed;
			std::span<const std::byte> payload{block.data() + 5, block.size() - 5};
			DecompressOptions opts;
			opts.max_output = 16ull * 1024 * 1024;
			auto out = (overrides_[idx].codec == Codec::None)
					   ? std::expected<std::vector<std::byte>, Error>{std::vector<std::byte>{
						     payload.begin(), payload.end()}}
					   : tagforge::decompress(payload, overrides_[idx].codec, opts);
			if (!out) {
				return std::unexpected(out.error());
			}
			cache_[idx] = *out;
		}
		return cache_[idx];
	}

	if (!cache_[idx].empty()) {
		return cache_[idx];
	}
	const auto loc = read_location(bytes_, idx);
	if (loc.sector_offset < 2 || loc.sector_count == 0) {
		return std::unexpected(make_error(ErrorCode::RegionInvalidChunk, idx, "chunk not present"));
	}
	const std::size_t start = static_cast<std::size_t>(loc.sector_offset) * kSector;
	if (start + 5 > bytes_.size()) {
		return std::unexpected(make_error(ErrorCode::RegionInvalidChunk, start, "chunk header truncated"));
	}
	const std::uint32_t len_field = read_u32_be(bytes_.data() + start);
	if (len_field == 0) {
		return std::unexpected(make_error(ErrorCode::RegionInvalidChunk, start, "zero-length chunk"));
	}
	const std::size_t payload_end = start + 4 + len_field;
	if (payload_end > bytes_.size()) {
		return std::unexpected(make_error(ErrorCode::RegionInvalidChunk, start, "chunk payload truncated"));
	}
	const auto codec_byte = static_cast<std::uint8_t>(bytes_[start + 4]);
	std::span<const std::byte> payload{bytes_.data() + start + 5, len_field - 1};

	Codec codec;
	switch (codec_byte) {
	case 1:
		codec = Codec::Gzip;
		break;
	case 2:
		codec = Codec::Zlib;
		break;
	case 3:
		codec = Codec::None;
		break;
	case 4:
		codec = Codec::Lz4;
		break;
	default:
		return std::unexpected(make_error(ErrorCode::UnknownCodec, start + 4));
	}

	DecompressOptions opts;
	opts.max_output = 16ull * 1024 * 1024;
	auto out = (codec == Codec::None)
			   ? std::expected<std::vector<std::byte>, Error>{std::vector<std::byte>{payload.begin(),
												 payload.end()}}
			   : tagforge::decompress(payload, codec, opts);
	if (!out) {
		return std::unexpected(out.error());
	}
	cache_[idx] = *out;
	return *out;
}

std::expected<NamedValue, Error> Region::chunk_value(int cx, int cz)
{
	auto raw = chunk_raw(cx, cz);
	if (!raw) {
		return std::unexpected(raw.error());
	}
	return tagforge::decode(*raw, Format::JavaNamedRoot);
}

std::expected<View, Error> Region::chunk_view(int cx, int cz)
{
	auto raw_ptr = chunk_raw(cx, cz);
	if (!raw_ptr) {
		return std::unexpected(raw_ptr.error());
	}
	const auto idx = chunk_index(cx, cz);
	return View::decode(cache_[idx], Format::JavaNamedRoot);
}

std::vector<std::pair<int, int>> Region::populated_chunks() const
{
	std::vector<std::pair<int, int>> out;
	for (int cz = 0; cz < kSize; ++cz) {
		for (int cx = 0; cx < kSize; ++cx) {
			if (has_chunk(cx, cz)) {
				out.emplace_back(cx, cz);
			}
		}
	}
	return out;
}

std::expected<void, Error> Region::write_chunk_bytes(int cx, int cz, std::span<const std::byte> named_root_nbt,
						     Codec codec, std::uint32_t timestamp_unix, int level)
{
	const auto idx = chunk_index(cx, cz);
	if (idx >= overrides_.size()) {
		overrides_.assign(kChunksPerRegion, ChunkEntry{});
	}
	auto block = make_chunk_block(named_root_nbt, codec, level);
	if (!block) {
		return std::unexpected(block.error());
	}
	overrides_[idx].compressed = std::move(*block);
	overrides_[idx].timestamp = timestamp_unix;
	overrides_[idx].codec = codec;
	overrides_[idx].present = true;
	cache_[idx].clear();
	return {};
}

std::expected<void, Error> Region::write_chunk(int cx, int cz, const NamedValue &nv, Codec codec,
					       std::uint32_t timestamp_unix, int level)
{
	auto encoded = encode(nv, Format::JavaNamedRoot);
	if (!encoded) {
		return std::unexpected(encoded.error());
	}
	return write_chunk_bytes(cx, cz, *encoded, codec, timestamp_unix, level);
}

void Region::remove_chunk(int cx, int cz) noexcept
{
	const auto idx = chunk_index(cx, cz);
	if (idx >= overrides_.size()) {
		return;
	}
	overrides_[idx].present = true;
	overrides_[idx].compressed.clear();
	overrides_[idx].timestamp = 0;
	overrides_[idx].codec = Codec::None;
	cache_[idx].clear();
}

std::expected<std::vector<std::byte>, Error> Region::save_to_bytes() const
{
	// Build a fresh file from scratch: 8 KiB header followed by chunk
	// blocks packed in row-major coordinate order, each padded to a
	// 4 KiB sector boundary.
	std::vector<std::byte> file(kHeaderBytes, std::byte{0});

	for (int cz = 0; cz < kSize; ++cz) {
		for (int cx = 0; cx < kSize; ++cx) {
			const auto idx = chunk_index(cx, cz);

			// Determine which bytes & timestamp to write for this slot.
			std::vector<std::byte> block;
			std::uint32_t timestamp = 0;

			const bool overridden = (idx < overrides_.size()) && overrides_[idx].present;
			const bool removed_by_override = overridden && overrides_[idx].compressed.empty();
			if (removed_by_override) {
				// Skip; location entry stays zero.
				continue;
			}
			if (overridden) {
				block = overrides_[idx].compressed;
				timestamp = overrides_[idx].timestamp;
			} else {
				// Fall back to the original file's chunk bytes.
				const auto loc = read_location(bytes_, idx);
				if (loc.sector_offset < 2 || loc.sector_count == 0) {
					continue;
				}
				const std::size_t start = static_cast<std::size_t>(loc.sector_offset) * kSector;
				if (start + 4 > bytes_.size()) {
					return std::unexpected(make_error(ErrorCode::RegionInvalidChunk, start));
				}
				const std::uint32_t len_field = read_u32_be(bytes_.data() + start);
				if (len_field == 0 || start + 4 + len_field > bytes_.size()) {
					return std::unexpected(make_error(ErrorCode::RegionInvalidChunk, start));
				}
				block.assign(bytes_.data() + start, bytes_.data() + start + 4 + len_field);
				timestamp = read_u32_be(bytes_.data() + kSector + idx * 4);
			}

			const std::size_t sector_offset = file.size() / kSector;
			const std::size_t sectors_needed = (block.size() + kSector - 1) / kSector;
			if (sector_offset > 0xFF'FFFFu || sectors_needed > 0xFFu) {
				return std::unexpected(make_error(ErrorCode::LengthOverflow, sector_offset,
								  "chunk too large for sector table"));
			}
			block.resize(sectors_needed * kSector); // pad with zero-init bytes
			file.insert(file.end(), block.begin(), block.end());

			const std::uint32_t loc_raw = (static_cast<std::uint32_t>(sector_offset) << 8) |
						      static_cast<std::uint8_t>(sectors_needed);
			write_u32_be(file.data() + idx * 4, loc_raw);
			write_u32_be(file.data() + kSector + idx * 4, timestamp);
		}
	}

	return file;
}

std::expected<void, Error> Region::save(const std::filesystem::path &path) const
{
	auto bytes = save_to_bytes();
	if (!bytes) {
		return std::unexpected(bytes.error());
	}
	return write_file(path, *bytes);
}

} // namespace tagforge
