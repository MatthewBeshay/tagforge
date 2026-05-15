// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace tagforge {

struct EndTag {
	[[nodiscard]] friend bool operator==(EndTag, EndTag) noexcept = default;
};

struct Value;
using Compound = std::vector<std::pair<std::string, Value>>;
using List = std::vector<Value>;

// The owning tree node. Variant arm index is congruent with TagId so that
// `kind()` and `v.index()` give the same answer; this allows fast switches.
//
// Strings (TAG_String) and Compound names are stored as valid UTF-8 - the
// MUTF-8 transcoding happens at the decode/encode boundary, not here.
struct Value {
	using Variant = std::variant<EndTag,                     //  0 -> TagId::End
				     std::int8_t,                //  1 -> TagId::Byte
				     std::int16_t,               //  2 -> TagId::Short
				     std::int32_t,               //  3 -> TagId::Int
				     std::int64_t,               //  4 -> TagId::Long
				     float,                      //  5 -> TagId::Float
				     double,                     //  6 -> TagId::Double
				     std::vector<std::int8_t>,   //  7 -> TagId::ByteArray
				     std::string,                //  8 -> TagId::String (UTF-8)
				     List,                       //  9 -> TagId::List
				     Compound,                   // 10 -> TagId::Compound
				     std::vector<std::int32_t>,  // 11 -> TagId::IntArray
				     std::vector<std::int64_t>>; // 12 -> TagId::LongArray

	Variant v{EndTag{}};

	[[nodiscard]] TagId kind() const noexcept { return static_cast<TagId>(v.index()); }

	// Valid iff kind() == TagId::List. Returns the arm tag of the
	// (homogeneous) list elements, or TagId::End for an empty list.
	[[nodiscard]] TagId list_arm() const noexcept;

	[[nodiscard]] friend bool operator==(const Value &, const Value &) = default;
};

// A named root, used by the JavaNamedRoot and Bedrock formats.
struct NamedValue {
	std::string name;
	Value value;

	[[nodiscard]] friend bool operator==(const NamedValue &, const NamedValue &) = default;
};

// Compound accessors are free functions (not methods) so Compound stays a
// transparent vector<pair> users can iterate or construct freely.

[[nodiscard]] const Value *find(const Compound &, std::string_view name) noexcept;
[[nodiscard]] Value *find(Compound &, std::string_view name) noexcept;

// Insert-or-replace. Returns a reference to the slot's value. Position of an
// existing key is preserved (this is what keeps round-trip byte-exact).
Value &upsert(Compound &, std::string name, Value value);

// Typed accessors. Each returns:
//   - the value when the key exists AND has the expected arm,
//   - ErrorCode::UnknownTagId with `detail="missing"` when the key isn't there,
//   - ErrorCode::UnexpectedRootType with `detail="wrong arm"` when the arm
//     mismatches (we reuse UnexpectedRootType for any "wrong tag at this
//     position" error rather than adding a code per arm).
[[nodiscard]] std::expected<std::int8_t, Error> get_byte(const Compound &, std::string_view);
[[nodiscard]] std::expected<std::int16_t, Error> get_short(const Compound &, std::string_view);
[[nodiscard]] std::expected<std::int32_t, Error> get_int(const Compound &, std::string_view);
[[nodiscard]] std::expected<std::int64_t, Error> get_long(const Compound &, std::string_view);
[[nodiscard]] std::expected<float, Error> get_float(const Compound &, std::string_view);
[[nodiscard]] std::expected<double, Error> get_double(const Compound &, std::string_view);
[[nodiscard]] std::expected<bool, Error> get_bool(const Compound &, std::string_view);
[[nodiscard]] std::expected<std::string_view, Error> get_string(const Compound &, std::string_view);
[[nodiscard]] std::expected<std::span<const std::int8_t>, Error> get_byte_array(const Compound &, std::string_view);
[[nodiscard]] std::expected<std::span<const std::int32_t>, Error> get_int_array(const Compound &, std::string_view);
[[nodiscard]] std::expected<std::span<const std::int64_t>, Error> get_long_array(const Compound &, std::string_view);
[[nodiscard]] std::expected<const Compound *, Error> get_compound(const Compound &, std::string_view);
[[nodiscard]] std::expected<const List *, Error> get_list(const Compound &, std::string_view);

} // namespace tagforge
