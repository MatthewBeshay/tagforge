// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// LZ4 Frame codec wrapper. Lives behind the same tagforge::compress facade as
// gzip/zlib; this is the only TU in tagforge that includes <lz4frame.h>.

#include "tagforge/compress.hpp"
#include "tagforge/error.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <expected>
#include <lz4frame.h>
#include <span>
#include <vector>

namespace tagforge {

namespace {

[[nodiscard]] std::expected<std::vector<std::byte>, Error> decompress_lz4(std::span<const std::byte> in,
									  std::size_t max_output)
{
	LZ4F_decompressionContext_t ctx = nullptr;
	if (LZ4F_isError(LZ4F_createDecompressionContext(&ctx, LZ4F_VERSION))) {
		return std::unexpected(make_error(ErrorCode::DecompressionFailed));
	}
	struct Guard {
		LZ4F_decompressionContext_t &c;
		~Guard() { LZ4F_freeDecompressionContext(c); }
	} g{ctx};

	std::vector<std::byte> out;
	const std::byte *in_ptr = in.data();
	std::size_t in_remaining = in.size();

	// Start with a healthy guess; grow on demand.
	std::size_t chunk = std::min<std::size_t>(64ull * 1024, max_output);
	if (chunk == 0) {
		chunk = 1024;
	}

	while (true) {
		const std::size_t out_off = out.size();
		if (out_off >= max_output) {
			return std::unexpected(
				make_error(ErrorCode::LimitExceeded, 0, "LZ4 output exceeds max_output"));
		}
		const std::size_t out_grow = std::min(chunk, max_output - out_off);
		out.resize(out_off + out_grow);

		std::size_t dst_size = out_grow;
		std::size_t src_size = in_remaining;
		const auto next_hint =
			LZ4F_decompress(ctx, out.data() + out_off, &dst_size, in_ptr, &src_size, nullptr);
		if (LZ4F_isError(next_hint)) {
			return std::unexpected(make_error(ErrorCode::DecompressionFailed));
		}

		out.resize(out_off + dst_size);
		in_ptr += src_size;
		in_remaining -= src_size;

		if (next_hint == 0) {
			// Frame fully consumed.
			return out;
		}
		if (in_remaining == 0) {
			return std::unexpected(make_error(ErrorCode::DecompressionFailed, 0, "LZ4 frame truncated"));
		}
		// Grow the chunk size for the next iteration.
		chunk = std::min<std::size_t>(chunk * 2, max_output);
	}
}

[[nodiscard]] std::expected<std::vector<std::byte>, Error> compress_lz4(std::span<const std::byte> in, int level)
{
	LZ4F_preferences_t prefs{};
	prefs.compressionLevel = std::clamp(level, 0, 12);

	const std::size_t bound = LZ4F_compressFrameBound(in.size(), &prefs);
	std::vector<std::byte> out(bound);
	const std::size_t actual = LZ4F_compressFrame(out.data(), out.size(), in.data(), in.size(), &prefs);
	if (LZ4F_isError(actual)) {
		return std::unexpected(make_error(ErrorCode::CompressionFailed));
	}
	out.resize(actual);
	return out;
}

} // namespace

namespace lz4_codec {

std::expected<std::vector<std::byte>, Error> decompress(std::span<const std::byte> bytes, std::size_t max_output)
{
	return decompress_lz4(bytes, max_output);
}

std::expected<std::vector<std::byte>, Error> compress(std::span<const std::byte> bytes, int level)
{
	return compress_lz4(bytes, level);
}

} // namespace lz4_codec

} // namespace tagforge
