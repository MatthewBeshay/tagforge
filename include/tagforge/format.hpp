// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace tagforge {

// The four wire shapes tagforge supports. Each is a concrete, named
// combination - there is no Java-LE or Bedrock-BE in the wild, so a flag
// struct would invite illegal states.
enum class Format : std::uint8_t {
	// Disk format, region-file chunk payload, classic .nbt / .dat files.
	// Big-endian integers, modified-UTF-8 strings, named root.
	JavaNamedRoot = 0,

	// 1.20.2+ Minecraft Java Edition network NBT. Same as JavaNamedRoot
	// except the root has no name (root tag is `tag_id` then payload).
	JavaAnonymousRoot = 1,

	// Bedrock Edition disk saves (level.dat etc.). Little-endian
	// integers and floats; uint16 length-prefixed strings; named root.
	BedrockLittleEndian = 2,

	// Bedrock Edition network NBT. Little-endian floats, but every
	// integer (including string lengths and array lengths) is a
	// ZigZag-encoded VarInt. Named root.
	BedrockVarInt = 3,
};

[[nodiscard]] constexpr bool is_big_endian(Format f) noexcept
{
	return f == Format::JavaNamedRoot || f == Format::JavaAnonymousRoot;
}

[[nodiscard]] constexpr bool has_root_name(Format f) noexcept
{
	return f != Format::JavaAnonymousRoot;
}

[[nodiscard]] constexpr bool uses_varint(Format f) noexcept
{
	return f == Format::BedrockVarInt;
}

[[nodiscard]] std::string_view format_name(Format f) noexcept;

// Heuristically inspect the first few bytes of an NBT buffer and guess its
// dialect. The check is best-effort:
//
//   * A leading 0x00 byte returns JavaAnonymousRoot (the "no NBT" sentinel).
//   * Otherwise the byte is interpreted as the root tag, and the name-length
//     encoding that follows is inspected to distinguish Java big-endian
//     (uint16 BE), Bedrock little-endian (uint16 LE), and Bedrock VarInt
//     (ZigZag VarInt) named roots.
//   * Anonymous-root Java network NBT shares its first byte with the named-root
//     dialects, so detect_format never returns JavaAnonymousRoot for non-zero
//     leading bytes; callers parsing network packets must pass the format
//     explicitly.
//
// Returns std::nullopt when the buffer is too short or the leading byte is not
// a valid TagId.
[[nodiscard]] std::optional<Format> detect_format(std::span<const std::byte> bytes) noexcept;

} // namespace tagforge
