// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include <cstdint>
#include <string_view>

namespace tagforge {

// NBT tag IDs as transmitted on the wire. The numeric values are wire-stable
// across every dialect tagforge supports.
enum class TagId : std::uint8_t {
	End = 0,
	Byte = 1,
	Short = 2,
	Int = 3,
	Long = 4,
	Float = 5,
	Double = 6,
	ByteArray = 7,
	String = 8,
	List = 9,
	Compound = 10,
	IntArray = 11,
	LongArray = 12,
};

[[nodiscard]] constexpr bool tag_id_is_valid(std::uint8_t raw) noexcept
{
	return raw <= 12;
}

[[nodiscard]] constexpr std::string_view tag_id_name(TagId id) noexcept
{
	switch (id) {
	case TagId::End:
		return "End";
	case TagId::Byte:
		return "Byte";
	case TagId::Short:
		return "Short";
	case TagId::Int:
		return "Int";
	case TagId::Long:
		return "Long";
	case TagId::Float:
		return "Float";
	case TagId::Double:
		return "Double";
	case TagId::ByteArray:
		return "ByteArray";
	case TagId::String:
		return "String";
	case TagId::List:
		return "List";
	case TagId::Compound:
		return "Compound";
	case TagId::IntArray:
		return "IntArray";
	case TagId::LongArray:
		return "LongArray";
	}
	return "Unknown";
}

} // namespace tagforge
