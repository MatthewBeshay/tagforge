// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tagforge {

// A parsed NBT path, modelled after the syntax accepted by Minecraft's `/data`
// command. Supported segments:
//
//   foo              compound key (must match the unquoted-identifier
//                    character class: [A-Za-z0-9_+.-])
//   .foo             chained compound key
//   ["foo"]          quoted compound key (any character allowed)
//   [N]              non-negative list / array index
//
// Filter syntax (`foo[{type:"chest"}]`) is **not** supported; that needs an
// SNBT subtree and is application-level.
//
// Example: parse_path("Items[0].tag.display.Name") yields five segments -
// {Key:"Items"}, {Index:0}, {Key:"tag"}, {Key:"display"}, {Key:"Name"}.
struct PathSegment {
	enum class Kind : std::uint8_t { Key, Index };
	Kind kind = Kind::Key;
	std::string key;       // valid iff kind == Key
	std::size_t index = 0; // valid iff kind == Index
};

class Path {
public:
	Path() = default;
	explicit Path(std::vector<PathSegment> segs) : segments_{std::move(segs)} {}

	[[nodiscard]] const std::vector<PathSegment> &segments() const noexcept { return segments_; }
	[[nodiscard]] bool empty() const noexcept { return segments_.empty(); }
	[[nodiscard]] std::size_t size() const noexcept { return segments_.size(); }

private:
	std::vector<PathSegment> segments_;
};

[[nodiscard]] std::expected<Path, Error> parse_path(std::string_view text);

// Walk `value` along `path` and return a pointer to the leaf Value. On any
// mismatch - missing key, non-compound traversal, index out of range, wrong
// arm - returns an Error indicating where the walk failed.
[[nodiscard]] std::expected<const Value *, Error> get_at(const Value &value, const Path &path) noexcept;
[[nodiscard]] std::expected<const Value *, Error> get_at(const Compound &compound, const Path &path) noexcept;

[[nodiscard]] std::expected<const Value *, Error> get_at(const Value &value, std::string_view path);
[[nodiscard]] std::expected<const Value *, Error> get_at(const Compound &compound, std::string_view path);

// Typed convenience accessors. Each returns the typed value when the walk
// resolves to that arm, otherwise UnknownTagId (missing) or UnexpectedRootType
// (wrong arm) propagates from get_at.
[[nodiscard]] std::expected<std::int8_t, Error> get_byte_at(const Value &, std::string_view);
[[nodiscard]] std::expected<std::int16_t, Error> get_short_at(const Value &, std::string_view);
[[nodiscard]] std::expected<std::int32_t, Error> get_int_at(const Value &, std::string_view);
[[nodiscard]] std::expected<std::int64_t, Error> get_long_at(const Value &, std::string_view);
[[nodiscard]] std::expected<float, Error> get_float_at(const Value &, std::string_view);
[[nodiscard]] std::expected<double, Error> get_double_at(const Value &, std::string_view);
[[nodiscard]] std::expected<std::string_view, Error> get_string_at(const Value &, std::string_view);

} // namespace tagforge
