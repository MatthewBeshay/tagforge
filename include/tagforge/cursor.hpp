// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <type_traits>

namespace tagforge {

// Concrete byte-cursor over a contiguous std::span. Every dialect tagforge
// supports is random-access over a buffer, so a concept-based byte_source
// abstraction was rejected.
struct Cursor {
	std::span<const std::byte> data{};
	std::size_t pos = 0;

	[[nodiscard]] constexpr std::size_t remaining() const noexcept { return data.size() - pos; }

	[[nodiscard]] constexpr bool empty() const noexcept { return pos >= data.size(); }

	[[nodiscard]] std::expected<void, Error> need(std::size_t n) const noexcept
	{
		if (remaining() < n) {
			return std::unexpected(make_error(ErrorCode::UnexpectedEndOfInput, pos));
		}
		return {};
	}

	[[nodiscard]] std::span<const std::byte> peek(std::size_t n) const noexcept { return data.subspan(pos, n); }

	[[nodiscard]] std::span<const std::byte> take(std::size_t n) noexcept
	{
		auto out = data.subspan(pos, n);
		pos += n;
		return out;
	}

	constexpr void skip(std::size_t n) noexcept { pos += n; }

	// Read a fixed-width integer in the requested endianness. Returns
	// ErrorCode::UnexpectedEndOfInput if fewer than sizeof(T) bytes remain.
	template<std::integral T> [[nodiscard]] std::expected<T, Error> read_int_be() noexcept
	{
		if (auto e = need(sizeof(T)); !e) {
			return std::unexpected(e.error());
		}
		T v{};
		std::memcpy(&v, data.data() + pos, sizeof(T));
		pos += sizeof(T);
		if constexpr (sizeof(T) > 1) {
			v = byteswap_if_le(v);
		}
		return v;
	}

	template<std::integral T> [[nodiscard]] std::expected<T, Error> read_int_le() noexcept
	{
		if (auto e = need(sizeof(T)); !e) {
			return std::unexpected(e.error());
		}
		T v{};
		std::memcpy(&v, data.data() + pos, sizeof(T));
		pos += sizeof(T);
		if constexpr (sizeof(T) > 1) {
			v = byteswap_if_be(v);
		}
		return v;
	}

	[[nodiscard]] std::expected<float, Error> read_float_be() noexcept;
	[[nodiscard]] std::expected<float, Error> read_float_le() noexcept;
	[[nodiscard]] std::expected<double, Error> read_double_be() noexcept;
	[[nodiscard]] std::expected<double, Error> read_double_le() noexcept;

private:
	template<std::integral T> static constexpr T byteswap_if_le(T v) noexcept
	{
		if constexpr (std::endian::native == std::endian::little && sizeof(T) > 1) {
			using U = std::make_unsigned_t<T>;
			return static_cast<T>(std::byteswap(static_cast<U>(v)));
		} else {
			return v;
		}
	}
	template<std::integral T> static constexpr T byteswap_if_be(T v) noexcept
	{
		if constexpr (std::endian::native == std::endian::big && sizeof(T) > 1) {
			using U = std::make_unsigned_t<T>;
			return static_cast<T>(std::byteswap(static_cast<U>(v)));
		} else {
			return v;
		}
	}
};

} // namespace tagforge
