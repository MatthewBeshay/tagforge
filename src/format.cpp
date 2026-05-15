// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "tagforge/format.hpp"

#include "tagforge/tag_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tagforge {

std::string_view format_name(Format f) noexcept
{
	switch (f) {
	case Format::JavaNamedRoot:
		return "JavaNamedRoot";
	case Format::JavaAnonymousRoot:
		return "JavaAnonymousRoot";
	case Format::BedrockLittleEndian:
		return "BedrockLittleEndian";
	case Format::BedrockVarInt:
		return "BedrockVarInt";
	}
	return "Unknown";
}

namespace {

[[nodiscard]] bool looks_like_printable(std::span<const std::byte> bytes) noexcept
{
	if (bytes.empty()) {
		return false;
	}
	for (auto b : bytes) {
		const auto c = static_cast<std::uint8_t>(b);
		// Permit ASCII printable + the limited high-bit MUTF-8 lead bytes
		// (0xC0..0xEF) that vanilla identifiers can technically contain
		// but never produce in practice.
		if (c < 0x20 || c >= 0x7F) {
			return false;
		}
	}
	return true;
}

} // namespace

std::optional<Format> detect_format(std::span<const std::byte> bytes) noexcept
{
	if (bytes.empty()) {
		return std::nullopt;
	}
	const auto b0 = static_cast<std::uint8_t>(bytes[0]);

	// Bare 0x00 byte: the "no NBT" sentinel used by 1.20.2+ network packets.
	if (b0 == 0x00) {
		return Format::JavaAnonymousRoot;
	}
	if (!tag_id_is_valid(b0)) {
		return std::nullopt;
	}
	if (bytes.size() < 3) {
		return std::nullopt;
	}

	const auto b1 = static_cast<std::uint8_t>(bytes[1]);
	const auto b2 = static_cast<std::uint8_t>(bytes[2]);

	// (b1, b2) is the start of the root-name length encoding for every
	// named-root dialect. Java BE uses uint16 BE; Bedrock LE uses uint16 LE;
	// Bedrock VarInt uses a continuation-bit-encoded VarInt that fits in
	// one byte for names up to 127 chars (i.e. all real-world names).

	// Java BE empty name (b1=b2=0) is the dominant case for `.nbt` / `.dat`
	// files. Bedrock LE empty name has the same byte pattern, so we tie-
	// break in favour of Java BE - Bedrock saves rarely have an empty root
	// name.
	if (b1 == 0x00 && b2 == 0x00) {
		return Format::JavaNamedRoot;
	}

	// Java BE non-empty name: b1 is the high byte of the uint16 length, so
	// it is 0 for names ≤ 255 chars (i.e. always in practice). If b1 == 0
	// and b2 != 0, treat as Java BE with name length = b2.
	if (b1 == 0x00) {
		return Format::JavaNamedRoot;
	}

	// b1 != 0 from here on.
	//
	// Bedrock VarInt with single-byte length (high bit clear): the name's
	// first byte sits at bytes[2]. Cross-check that the apparent name body
	// looks like a printable identifier.
	if ((b1 & 0x80u) == 0) {
		const std::size_t name_len = b1;
		if (bytes.size() >= 2 + name_len) {
			std::span<const std::byte> varint_name{bytes.data() + 2, name_len};
			if (looks_like_printable(varint_name)) {
				return Format::BedrockVarInt;
			}
		}
		// Fall through to Bedrock LE: uint16 LE name length = b1 | (b2 << 8).
		const std::size_t le_name_len = static_cast<std::size_t>(b1) | (static_cast<std::size_t>(b2) << 8);
		if (bytes.size() >= 3 + le_name_len) {
			std::span<const std::byte> le_name{bytes.data() + 3, le_name_len};
			if (looks_like_printable(le_name)) {
				return Format::BedrockLittleEndian;
			}
		}
		// Last resort: pick BedrockLittleEndian - it's the on-disk Bedrock
		// shape we'd most likely receive without further context.
		return Format::BedrockLittleEndian;
	}

	// b1 >= 0x80: VarInt continuation bit is set, so this is a multi-byte
	// VarInt length (name ≥ 128 chars). Rare but unambiguous for Bedrock.
	return Format::BedrockVarInt;
}

} // namespace tagforge
