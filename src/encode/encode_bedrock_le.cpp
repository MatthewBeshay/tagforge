// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "../internal/encode_internal.hpp"

#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace tagforge::detail {

namespace {

void write_u8(std::vector<std::byte> &out, std::uint8_t v)
{
	out.push_back(static_cast<std::byte>(v));
}

template<std::integral T> void write_int_le(std::vector<std::byte> &out, T v)
{
	using U = std::make_unsigned_t<T>;
	U u = static_cast<U>(v);
	if constexpr (sizeof(T) > 1) {
		if constexpr (std::endian::native == std::endian::big) {
			u = std::byteswap(u);
		}
	}
	const std::byte *src = reinterpret_cast<const std::byte *>(&u);
	out.insert(out.end(), src, src + sizeof(U));
}

void write_float_le(std::vector<std::byte> &out, float f)
{
	write_int_le<std::uint32_t>(out, std::bit_cast<std::uint32_t>(f));
}
void write_double_le(std::vector<std::byte> &out, double d)
{
	write_int_le<std::uint64_t>(out, std::bit_cast<std::uint64_t>(d));
}

[[nodiscard]] std::expected<void, Error> write_string(std::vector<std::byte> &out, std::string_view s)
{
	if (s.size() > std::numeric_limits<std::uint16_t>::max()) {
		return std::unexpected(make_error(ErrorCode::LengthOverflow));
	}
	write_int_le<std::uint16_t>(out, static_cast<std::uint16_t>(s.size()));
	const std::byte *src = reinterpret_cast<const std::byte *>(s.data());
	out.insert(out.end(), src, src + s.size());
	return {};
}

[[nodiscard]] std::expected<void, Error> write_array_length(std::vector<std::byte> &out, std::size_t n)
{
	if (n > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
		return std::unexpected(make_error(ErrorCode::LengthOverflow));
	}
	write_int_le<std::int32_t>(out, static_cast<std::int32_t>(n));
	return {};
}

[[nodiscard]] std::expected<void, Error> write_payload(std::vector<std::byte> &out, const Value &v);

[[nodiscard]] std::expected<void, Error> write_payload(std::vector<std::byte> &out, const Value &v)
{
	switch (v.kind()) {
	case TagId::End:
		return {};
	case TagId::Byte:
		write_int_le<std::int8_t>(out, std::get<std::int8_t>(v.v));
		return {};
	case TagId::Short:
		write_int_le<std::int16_t>(out, std::get<std::int16_t>(v.v));
		return {};
	case TagId::Int:
		write_int_le<std::int32_t>(out, std::get<std::int32_t>(v.v));
		return {};
	case TagId::Long:
		write_int_le<std::int64_t>(out, std::get<std::int64_t>(v.v));
		return {};
	case TagId::Float:
		write_float_le(out, std::get<float>(v.v));
		return {};
	case TagId::Double:
		write_double_le(out, std::get<double>(v.v));
		return {};
	case TagId::ByteArray: {
		const auto &arr = std::get<std::vector<std::int8_t>>(v.v);
		if (auto e = write_array_length(out, arr.size()); !e) {
			return e;
		}
		const std::byte *src = reinterpret_cast<const std::byte *>(arr.data());
		out.insert(out.end(), src, src + arr.size());
		return {};
	}
	case TagId::String:
		return write_string(out, std::get<std::string>(v.v));
	case TagId::List: {
		const auto &list = std::get<List>(v.v);
		const TagId arm = list.empty() ? TagId::End : list.front().kind();
		write_u8(out, static_cast<std::uint8_t>(arm));
		if (auto e = write_array_length(out, list.size()); !e) {
			return e;
		}
		for (const auto &elem : list) {
			if (elem.kind() != arm) {
				return std::unexpected(make_error(ErrorCode::MixedListArm));
			}
			if (auto r = write_payload(out, elem); !r) {
				return r;
			}
		}
		return {};
	}
	case TagId::Compound: {
		const auto &c = std::get<Compound>(v.v);
		for (const auto &[name, child] : c) {
			write_u8(out, static_cast<std::uint8_t>(child.kind()));
			if (auto r = write_string(out, name); !r) {
				return r;
			}
			if (auto r = write_payload(out, child); !r) {
				return r;
			}
		}
		write_u8(out, 0x00);
		return {};
	}
	case TagId::IntArray: {
		const auto &arr = std::get<std::vector<std::int32_t>>(v.v);
		if (auto e = write_array_length(out, arr.size()); !e) {
			return e;
		}
		for (std::int32_t x : arr) {
			write_int_le<std::int32_t>(out, x);
		}
		return {};
	}
	case TagId::LongArray: {
		const auto &arr = std::get<std::vector<std::int64_t>>(v.v);
		if (auto e = write_array_length(out, arr.size()); !e) {
			return e;
		}
		for (std::int64_t x : arr) {
			write_int_le<std::int64_t>(out, x);
		}
		return {};
	}
	}
	return std::unexpected(make_error(ErrorCode::UnknownTagId));
}

} // namespace

std::expected<void, Error> encode_bedrock_le(const NamedValue &nv, std::vector<std::byte> &out)
{
	if (nv.value.kind() == TagId::End) {
		write_u8(out, 0x00);
		return {};
	}
	write_u8(out, static_cast<std::uint8_t>(nv.value.kind()));
	if (auto r = write_string(out, nv.name); !r) {
		return r;
	}
	return write_payload(out, nv.value);
}

} // namespace tagforge::detail
