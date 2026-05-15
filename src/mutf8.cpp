// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "tagforge/mutf8.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace tagforge {

namespace {

constexpr std::uint8_t kCont6 = 0b1000'0000; // continuation byte tag
constexpr std::uint8_t kCont6Mask = 0b1100'0000;

[[nodiscard]] bool is_cont(std::uint8_t b) noexcept
{
	return (b & kCont6Mask) == kCont6;
}

[[nodiscard]] inline std::uint8_t to_byte(char c) noexcept
{
	return static_cast<std::uint8_t>(c);
}

// Encode `cp` as UTF-8 into `out`. Caller guarantees cp ≤ 0x10FFFF and not a
// surrogate (the latter is allowed only inside MUTF-8 surrogate pairs which
// are merged into a single non-surrogate codepoint before they reach here).
void encode_utf8(std::uint32_t cp, std::string &out)
{
	if (cp < 0x80) {
		out.push_back(static_cast<char>(cp));
	} else if (cp < 0x800) {
		out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	} else if (cp < 0x10000) {
		out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	} else {
		out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
}

} // namespace

bool is_pure_ascii(std::span<const std::byte> bytes) noexcept
{
	for (auto b : bytes) {
		const auto u = static_cast<std::uint8_t>(b);
		if (u == 0 || u >= 0x80) {
			return false;
		}
	}
	return true;
}

bool is_pure_ascii(std::string_view bytes) noexcept
{
	for (char c : bytes) {
		const auto u = to_byte(c);
		if (u == 0 || u >= 0x80) {
			return false;
		}
	}
	return true;
}

std::expected<std::string, Error> mutf8_to_utf8(std::span<const std::byte> bytes)
{
	std::string out;
	out.reserve(bytes.size());

	const std::size_t n = bytes.size();
	std::size_t i = 0;

	auto raw = [&](std::size_t idx) {
		return static_cast<std::uint8_t>(bytes[idx]);
	};

	while (i < n) {
		const std::uint8_t b0 = raw(i);

		if (b0 == 0x00) {
			return std::unexpected(
				make_error(ErrorCode::InvalidMutf8, i, "bare 0x00 disallowed in MUTF-8"));
		}

		if (b0 < 0x80) {
			out.push_back(static_cast<char>(b0));
			++i;
			continue;
		}

		// 2-byte sequence: 110xxxxx 10xxxxxx
		if ((b0 & 0xE0) == 0xC0) {
			if (i + 1 >= n) {
				return std::unexpected(make_error(ErrorCode::UnexpectedEndOfInput, i));
			}
			const std::uint8_t b1 = raw(i + 1);
			if (!is_cont(b1)) {
				return std::unexpected(
					make_error(ErrorCode::InvalidMutf8, i + 1, "expected continuation byte"));
			}
			// Special-case 0xC0 0x80 → U+0000.
			if (b0 == 0xC0 && b1 == 0x80) {
				out.push_back('\0');
				i += 2;
				continue;
			}
			// Reject overlong: must have b0 >= 0xC2 for non-NUL.
			if (b0 < 0xC2) {
				return std::unexpected(
					make_error(ErrorCode::InvalidMutf8, i, "overlong 2-byte sequence"));
			}
			const std::uint32_t cp = (static_cast<std::uint32_t>(b0 & 0x1Fu) << 6) |
						 static_cast<std::uint32_t>(b1 & 0x3Fu);
			encode_utf8(cp, out);
			i += 2;
			continue;
		}

		// 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
		if ((b0 & 0xF0) == 0xE0) {
			if (i + 2 >= n) {
				return std::unexpected(make_error(ErrorCode::UnexpectedEndOfInput, i));
			}
			const std::uint8_t b1 = raw(i + 1);
			const std::uint8_t b2 = raw(i + 2);
			if (!is_cont(b1) || !is_cont(b2)) {
				return std::unexpected(
					make_error(ErrorCode::InvalidMutf8, i, "expected continuation byte"));
			}
			const std::uint32_t cp = (static_cast<std::uint32_t>(b0 & 0x0Fu) << 12) |
						 (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6) |
						 static_cast<std::uint32_t>(b2 & 0x3Fu);
			if (cp < 0x800) {
				return std::unexpected(
					make_error(ErrorCode::InvalidMutf8, i, "overlong 3-byte sequence"));
			}
			// Surrogate range U+D800..U+DFFF.
			if (cp >= 0xD800 && cp <= 0xDFFF) {
				if (cp <= 0xDBFF) {
					// High surrogate; expect a low surrogate next as 3-byte MUTF-8.
					if (i + 5 >= n) {
						return std::unexpected(make_error(ErrorCode::InvalidMutf8, i,
										  "unpaired high surrogate"));
					}
					const std::uint8_t c0 = raw(i + 3);
					const std::uint8_t c1 = raw(i + 4);
					const std::uint8_t c2 = raw(i + 5);
					if ((c0 & 0xF0) != 0xE0 || !is_cont(c1) || !is_cont(c2)) {
						return std::unexpected(make_error(ErrorCode::InvalidMutf8, i + 3,
										  "expected low surrogate triple"));
					}
					const std::uint32_t low = (static_cast<std::uint32_t>(c0 & 0x0Fu) << 12) |
								  (static_cast<std::uint32_t>(c1 & 0x3Fu) << 6) |
								  static_cast<std::uint32_t>(c2 & 0x3Fu);
					if (low < 0xDC00 || low > 0xDFFF) {
						return std::unexpected(make_error(ErrorCode::InvalidMutf8, i + 3,
										  "expected low surrogate value"));
					}
					const std::uint32_t merged =
						0x10000U + ((cp - 0xD800U) << 10) + (low - 0xDC00U);
					encode_utf8(merged, out);
					i += 6;
					continue;
				}
				return std::unexpected(make_error(ErrorCode::InvalidMutf8, i, "lone low surrogate"));
			}
			encode_utf8(cp, out);
			i += 3;
			continue;
		}

		// 4-byte UTF-8 sequences are not valid MUTF-8.
		return std::unexpected(make_error(ErrorCode::InvalidMutf8, i, "4-byte sequence disallowed in MUTF-8"));
	}

	return out;
}

std::expected<std::string, Error> mutf8_to_utf8(std::string_view bytes)
{
	return mutf8_to_utf8(std::span{reinterpret_cast<const std::byte *>(bytes.data()), bytes.size()});
}

std::expected<std::string, Error> utf8_to_mutf8(std::string_view utf8)
{
	std::string out;
	out.reserve(utf8.size());

	const std::size_t n = utf8.size();
	std::size_t i = 0;

	auto raw = [&](std::size_t idx) {
		return to_byte(utf8[idx]);
	};

	while (i < n) {
		const std::uint8_t b0 = raw(i);

		if (b0 == 0x00) {
			out.push_back(static_cast<char>(0xC0));
			out.push_back(static_cast<char>(0x80));
			++i;
			continue;
		}
		if (b0 < 0x80) {
			out.push_back(static_cast<char>(b0));
			++i;
			continue;
		}

		if ((b0 & 0xE0) == 0xC0) {
			if (i + 1 >= n) {
				return std::unexpected(make_error(ErrorCode::UnexpectedEndOfInput, i));
			}
			const std::uint8_t b1 = raw(i + 1);
			if (!is_cont(b1) || b0 < 0xC2) {
				return std::unexpected(make_error(ErrorCode::InvalidUtf8, i));
			}
			out.push_back(static_cast<char>(b0));
			out.push_back(static_cast<char>(b1));
			i += 2;
			continue;
		}

		if ((b0 & 0xF0) == 0xE0) {
			if (i + 2 >= n) {
				return std::unexpected(make_error(ErrorCode::UnexpectedEndOfInput, i));
			}
			const std::uint8_t b1 = raw(i + 1);
			const std::uint8_t b2 = raw(i + 2);
			if (!is_cont(b1) || !is_cont(b2)) {
				return std::unexpected(make_error(ErrorCode::InvalidUtf8, i));
			}
			const std::uint32_t cp = (static_cast<std::uint32_t>(b0 & 0x0Fu) << 12) |
						 (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6) |
						 static_cast<std::uint32_t>(b2 & 0x3Fu);
			if (cp < 0x800) {
				return std::unexpected(
					make_error(ErrorCode::InvalidUtf8, i, "overlong 3-byte sequence"));
			}
			// Surrogates are disallowed in standard UTF-8.
			if (cp >= 0xD800 && cp <= 0xDFFF) {
				return std::unexpected(make_error(ErrorCode::InvalidUtf8, i, "surrogate in UTF-8"));
			}
			out.push_back(static_cast<char>(b0));
			out.push_back(static_cast<char>(b1));
			out.push_back(static_cast<char>(b2));
			i += 3;
			continue;
		}

		if ((b0 & 0xF8) == 0xF0) {
			if (i + 3 >= n) {
				return std::unexpected(make_error(ErrorCode::UnexpectedEndOfInput, i));
			}
			const std::uint8_t b1 = raw(i + 1);
			const std::uint8_t b2 = raw(i + 2);
			const std::uint8_t b3 = raw(i + 3);
			if (!is_cont(b1) || !is_cont(b2) || !is_cont(b3)) {
				return std::unexpected(make_error(ErrorCode::InvalidUtf8, i));
			}
			const std::uint32_t cp = (static_cast<std::uint32_t>(b0 & 0x07u) << 18) |
						 (static_cast<std::uint32_t>(b1 & 0x3Fu) << 12) |
						 (static_cast<std::uint32_t>(b2 & 0x3Fu) << 6) |
						 static_cast<std::uint32_t>(b3 & 0x3Fu);
			if (cp < 0x10000 || cp > 0x10FFFF) {
				return std::unexpected(
					make_error(ErrorCode::InvalidUtf8, i, "4-byte sequence out of range"));
			}
			const std::uint32_t adj = cp - 0x10000U;
			const std::uint32_t high = 0xD800U + (adj >> 10);
			const std::uint32_t low = 0xDC00U + (adj & 0x3FFU);
			encode_utf8(high, out); // encodes as 3-byte form
			encode_utf8(low, out);  // encodes as 3-byte form
			i += 4;
			continue;
		}

		return std::unexpected(make_error(ErrorCode::InvalidUtf8, i, "invalid leading byte"));
	}

	return out;
}

} // namespace tagforge
