// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// The single TU permitted to include <libdeflate.h>. All callers go through
// the Codec-based facade in tagforge/compress.hpp.

#include "tagforge/compress.hpp"
#include "tagforge/error.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <libdeflate.h>
#include <memory>
#include <span>
#include <vector>

namespace tagforge {

namespace {

struct DecompressorDeleter {
	void operator()(libdeflate_decompressor *p) const noexcept { libdeflate_free_decompressor(p); }
};
struct CompressorDeleter {
	void operator()(libdeflate_compressor *p) const noexcept { libdeflate_free_compressor(p); }
};
using DecompressorPtr = std::unique_ptr<libdeflate_decompressor, DecompressorDeleter>;
using CompressorPtr = std::unique_ptr<libdeflate_compressor, CompressorDeleter>;

[[nodiscard]] std::expected<std::vector<std::byte>, Error> decompress_gzip(std::span<const std::byte> in,
									   std::size_t max_output)
{
	// gzip stores the uncompressed size mod 2^32 in the last 4 bytes (ISIZE).
	// Use it as a starting estimate; fall back to a growth loop if it
	// disagrees (huge files where ISIZE wraps).
	std::size_t guess = 0;
	if (in.size() >= 4) {
		std::uint32_t isize = 0;
		std::memcpy(&isize, in.data() + in.size() - 4, 4);
		// ISIZE is little-endian.
		if (std::endian::native == std::endian::big) {
			isize = std::byteswap(isize);
		}
		guess = isize;
	}
	if (guess == 0 || guess > max_output) {
		guess = std::min<std::size_t>(in.size() * 8, max_output);
		if (guess == 0) {
			guess = 4096;
		}
	}

	DecompressorPtr d{libdeflate_alloc_decompressor()};
	if (!d) {
		return std::unexpected(make_error(ErrorCode::DecompressionFailed));
	}

	std::vector<std::byte> out;
	while (true) {
		out.resize(guess);
		std::size_t actual = 0;
		const auto rc =
			libdeflate_gzip_decompress(d.get(), in.data(), in.size(), out.data(), out.size(), &actual);
		if (rc == LIBDEFLATE_SUCCESS) {
			out.resize(actual);
			return out;
		}
		if (rc == LIBDEFLATE_INSUFFICIENT_SPACE) {
			if (guess >= max_output) {
				return std::unexpected(
					make_error(ErrorCode::LimitExceeded, 0, "gzip output exceeds max_output"));
			}
			guess = std::min(max_output, guess * 2 + 1);
			continue;
		}
		return std::unexpected(make_error(ErrorCode::DecompressionFailed));
	}
}

[[nodiscard]] std::expected<std::vector<std::byte>, Error> decompress_zlib(std::span<const std::byte> in,
									   std::size_t max_output)
{
	DecompressorPtr d{libdeflate_alloc_decompressor()};
	if (!d) {
		return std::unexpected(make_error(ErrorCode::DecompressionFailed));
	}
	std::size_t guess = std::min<std::size_t>(std::max<std::size_t>(in.size() * 4, 4096), max_output);
	std::vector<std::byte> out;
	while (true) {
		out.resize(guess);
		std::size_t actual = 0;
		const auto rc =
			libdeflate_zlib_decompress(d.get(), in.data(), in.size(), out.data(), out.size(), &actual);
		if (rc == LIBDEFLATE_SUCCESS) {
			out.resize(actual);
			return out;
		}
		if (rc == LIBDEFLATE_INSUFFICIENT_SPACE) {
			if (guess >= max_output) {
				return std::unexpected(
					make_error(ErrorCode::LimitExceeded, 0, "zlib output exceeds max_output"));
			}
			guess = std::min(max_output, guess * 2 + 1);
			continue;
		}
		return std::unexpected(make_error(ErrorCode::DecompressionFailed));
	}
}

} // namespace

// Implemented in lz4_wrap.cpp.
namespace lz4_codec {
std::expected<std::vector<std::byte>, Error> decompress(std::span<const std::byte>, std::size_t max_output);
std::expected<std::vector<std::byte>, Error> compress(std::span<const std::byte>, int level);
} // namespace lz4_codec

std::expected<std::vector<std::byte>, Error> decompress(std::span<const std::byte> bytes, const DecompressOptions &opts)
{
	auto codec = detect_codec(bytes);
	if (!codec) {
		// Raw bytes; copy through.
		return std::vector<std::byte>{bytes.begin(), bytes.end()};
	}
	return decompress(bytes, *codec, opts);
}

std::expected<std::vector<std::byte>, Error> decompress(std::span<const std::byte> bytes, Codec codec,
							const DecompressOptions &opts)
{
	switch (codec) {
	case Codec::None:
		return std::vector<std::byte>{bytes.begin(), bytes.end()};
	case Codec::Gzip:
		return decompress_gzip(bytes, opts.max_output);
	case Codec::Zlib:
		return decompress_zlib(bytes, opts.max_output);
	case Codec::Lz4:
		return lz4_codec::decompress(bytes, opts.max_output);
	}
	return std::unexpected(make_error(ErrorCode::UnknownCodec));
}

std::expected<std::vector<std::byte>, Error> compress(std::span<const std::byte> bytes, Codec codec, int level)
{
	if (codec == Codec::None) {
		return std::vector<std::byte>{bytes.begin(), bytes.end()};
	}
	if (codec == Codec::Lz4) {
		return lz4_codec::compress(bytes, level);
	}
	level = std::clamp(level, 1, 12);
	CompressorPtr c{libdeflate_alloc_compressor(level)};
	if (!c) {
		return std::unexpected(make_error(ErrorCode::CompressionFailed));
	}
	const std::size_t bound = (codec == Codec::Gzip) ? libdeflate_gzip_compress_bound(c.get(), bytes.size())
							 : libdeflate_zlib_compress_bound(c.get(), bytes.size());
	std::vector<std::byte> out(bound);
	const std::size_t actual =
		(codec == Codec::Gzip)
			? libdeflate_gzip_compress(c.get(), bytes.data(), bytes.size(), out.data(), out.size())
			: libdeflate_zlib_compress(c.get(), bytes.data(), bytes.size(), out.data(), out.size());
	if (actual == 0) {
		return std::unexpected(make_error(ErrorCode::CompressionFailed));
	}
	out.resize(actual);
	return out;
}

} // namespace tagforge
