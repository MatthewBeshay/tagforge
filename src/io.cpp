// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "tagforge/io.hpp"

#include "tagforge/compress.hpp"
#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace tagforge {

std::expected<std::vector<std::byte>, Error> read_file(const std::filesystem::path &path)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f.is_open()) {
		return std::unexpected(make_error(ErrorCode::Io, 0, "could not open file"));
	}
	const std::streamsize n = f.tellg();
	if (n < 0) {
		return std::unexpected(make_error(ErrorCode::Io));
	}
	std::vector<std::byte> out(static_cast<std::size_t>(n));
	f.seekg(0);
	f.read(reinterpret_cast<char *>(out.data()), n);
	if (!f && !f.eof()) {
		return std::unexpected(make_error(ErrorCode::Io));
	}
	return out;
}

std::expected<void, Error> write_file(const std::filesystem::path &path, std::span<const std::byte> bytes)
{
	std::error_code mkdir_ec;
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path(), mkdir_ec);
		if (mkdir_ec) {
			return std::unexpected(make_error(ErrorCode::Io, 0, "could not create parent directory"));
		}
	}
	std::ofstream f(path, std::ios::binary | std::ios::trunc);
	if (!f.is_open()) {
		return std::unexpected(make_error(ErrorCode::Io, 0, "could not open file for writing"));
	}
	f.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!f) {
		return std::unexpected(make_error(ErrorCode::Io));
	}
	return {};
}

std::expected<NamedValue, Error> read_nbt_file(const std::filesystem::path &path)
{
	auto raw = read_file(path);
	if (!raw) {
		return std::unexpected(raw.error());
	}
	auto decompressed = decompress(*raw);
	if (!decompressed) {
		return std::unexpected(decompressed.error());
	}
	return decode_auto(*decompressed);
}

std::expected<void, Error> write_nbt_file(const std::filesystem::path &path, const NamedValue &nv, Format format,
					  Codec codec, int level)
{
	auto encoded = encode(nv, format);
	if (!encoded) {
		return std::unexpected(encoded.error());
	}
	if (codec == Codec::None) {
		return write_file(path, *encoded);
	}
	auto compressed = compress(*encoded, codec, level);
	if (!compressed) {
		return std::unexpected(compressed.error());
	}
	return write_file(path, *compressed);
}

} // namespace tagforge
