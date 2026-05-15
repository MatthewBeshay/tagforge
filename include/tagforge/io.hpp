// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/compress.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <vector>

namespace tagforge {

// File-level convenience built on top of decode / encode / decompress /
// compress. Each call performs one filesystem operation plus the in-memory
// transformations; no extra state is retained between calls.

// Read a file into a byte buffer.
[[nodiscard]] std::expected<std::vector<std::byte>, Error> read_file(const std::filesystem::path &path);

// Write a byte buffer to a file. Overwrites if it exists; creates any missing
// parent directories.
[[nodiscard]] std::expected<void, Error> write_file(const std::filesystem::path &path,
						    std::span<const std::byte> bytes);

// Read an NBT file from disk:
//   * autodetect gzip / zlib / raw via tagforge::detect_codec,
//   * decompress if needed,
//   * autodetect Java BE / Bedrock LE / Bedrock VarInt via tagforge::detect_format,
//   * decode into an owning NamedValue tree.
//
// If you already know the format and/or codec, prefer the lower-level
// `decode(bytes, format)` + `decompress(bytes, codec)` directly - they take
// the same amount of code and avoid the heuristic.
[[nodiscard]] std::expected<NamedValue, Error> read_nbt_file(const std::filesystem::path &path);

// Write an NBT tree to a file, encoding with `format` and optionally wrapping
// in `codec`. Pass Codec::None for raw bytes (the most portable choice for
// non-Minecraft consumers).
[[nodiscard]] std::expected<void, Error> write_nbt_file(const std::filesystem::path &path, const NamedValue &nv,
							Format format = Format::JavaNamedRoot,
							Codec codec = Codec::None,
							int compression_level = compression_level::balanced);

} // namespace tagforge
