// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Out-of-line bodies for the float/double cursor wrappers. The integer reads
// are inline templates in cursor.hpp; these float/double helpers just
// bit-cast the result of an unsigned-integer read at the matching endianness.

#include "tagforge/cursor.hpp"

#include <bit>
#include <cstdint>

namespace tagforge {

std::expected<float, Error> Cursor::read_float_be() noexcept
{
	auto bits = read_int_be<std::uint32_t>();
	if (!bits) {
		return std::unexpected(bits.error());
	}
	return std::bit_cast<float>(*bits);
}

std::expected<float, Error> Cursor::read_float_le() noexcept
{
	auto bits = read_int_le<std::uint32_t>();
	if (!bits) {
		return std::unexpected(bits.error());
	}
	return std::bit_cast<float>(*bits);
}

std::expected<double, Error> Cursor::read_double_be() noexcept
{
	auto bits = read_int_be<std::uint64_t>();
	if (!bits) {
		return std::unexpected(bits.error());
	}
	return std::bit_cast<double>(*bits);
}

std::expected<double, Error> Cursor::read_double_le() noexcept
{
	auto bits = read_int_le<std::uint64_t>();
	if (!bits) {
		return std::unexpected(bits.error());
	}
	return std::bit_cast<double>(*bits);
}

} // namespace tagforge
