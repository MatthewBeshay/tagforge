// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "tagforge/path.hpp"

#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace tagforge {

namespace {

constexpr bool is_unquoted_id_char(char c) noexcept
{
	// Mojang's path syntax reserves '.' as the segment separator, so it
	// is NOT part of the unquoted-identifier class. NBT keys that contain
	// a literal '.' must be referenced via the quoted form: `["a.b"]`.
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
	       c == '+';
}

} // namespace

std::expected<Path, Error> parse_path(std::string_view text)
{
	std::vector<PathSegment> segs;
	std::size_t i = 0;
	const std::size_t n = text.size();

	auto syntax = [&](std::string_view detail) {
		return make_error(ErrorCode::SnbtSyntax, i, detail);
	};

	while (i < n) {
		// Optional leading '.' between key segments after the first.
		if (!segs.empty() && text[i] == '.') {
			++i;
			if (i >= n) {
				return std::unexpected(syntax("path ends with '.'"));
			}
		}

		if (text[i] == '[') {
			++i;
			if (i >= n) {
				return std::unexpected(syntax("unterminated '['"));
			}
			if (text[i] == '"' || text[i] == '\'') {
				const char quote = text[i++];
				std::string key;
				while (i < n && text[i] != quote) {
					if (text[i] == '\\' && i + 1 < n) {
						key.push_back(text[i + 1]);
						i += 2;
						continue;
					}
					key.push_back(text[i++]);
				}
				if (i >= n) {
					return std::unexpected(syntax("unterminated quoted key"));
				}
				++i; // closing quote
				if (i >= n || text[i] != ']') {
					return std::unexpected(syntax("expected ']' after quoted key"));
				}
				++i; // closing bracket
				segs.push_back(PathSegment{.kind = PathSegment::Kind::Key, .key = std::move(key)});
				continue;
			}
			// Numeric index.
			const std::size_t start = i;
			while (i < n && std::isdigit(static_cast<unsigned char>(text[i]))) {
				++i;
			}
			if (i == start) {
				return std::unexpected(syntax("expected number or quoted key after '['"));
			}
			std::size_t idx = 0;
			const char *p = text.data() + start;
			auto [end, ec] = std::from_chars(p, text.data() + i, idx);
			if (ec != std::errc{}) {
				return std::unexpected(make_error(ErrorCode::SnbtNumberOutOfRange, start));
			}
			if (i >= n || text[i] != ']') {
				return std::unexpected(syntax("expected ']'"));
			}
			++i;
			segs.push_back(PathSegment{.kind = PathSegment::Kind::Index, .key = {}, .index = idx});
			continue;
		}

		// Unquoted identifier.
		const std::size_t start = i;
		while (i < n && is_unquoted_id_char(text[i])) {
			++i;
		}
		if (i == start) {
			return std::unexpected(syntax("expected identifier"));
		}
		segs.push_back(PathSegment{
			.kind = PathSegment::Kind::Key,
			.key = std::string{text.substr(start, i - start)},
		});
	}

	return Path{std::move(segs)};
}

namespace {

[[nodiscard]] Error not_found_error(std::size_t step, std::string_view detail)
{
	return make_error(ErrorCode::UnknownTagId, step, detail);
}

[[nodiscard]] Error wrong_type_error(std::size_t step, std::string_view detail)
{
	return make_error(ErrorCode::UnexpectedRootType, step, detail);
}

[[nodiscard]] std::expected<const Value *, Error> walk_from(const Value *cur, const Path &path,
							    std::size_t start_index) noexcept
{
	for (std::size_t i = start_index; i < path.segments().size(); ++i) {
		const auto &seg = path.segments()[i];
		if (seg.kind == PathSegment::Kind::Key) {
			if (cur->kind() != TagId::Compound) {
				return std::unexpected(wrong_type_error(i, "path step expected Compound"));
			}
			const Compound &c = std::get<Compound>(cur->v);
			const Value *next = find(c, seg.key);
			if (!next) {
				return std::unexpected(not_found_error(i, "key not found"));
			}
			cur = next;
		} else {
			// Index step: List only. Primitive arrays (ByteArray,
			// IntArray, LongArray) carry plain integers that don't
			// fit in a Value with stable lifetime - callers should
			// fetch the array via the typed accessor and index it.
			switch (cur->kind()) {
			case TagId::List: {
				const auto &list = std::get<List>(cur->v);
				if (seg.index >= list.size()) {
					return std::unexpected(
						make_error(ErrorCode::LengthOverflow, i, "list index out of range"));
				}
				cur = &list[seg.index];
				break;
			}
			case TagId::ByteArray:
			case TagId::IntArray:
			case TagId::LongArray:
				return std::unexpected(wrong_type_error(
					i, "cannot index primitive arrays through path; use array accessors"));
			default:
				return std::unexpected(wrong_type_error(i, "path index step expected List"));
			}
		}
	}
	return cur;
}

} // namespace

std::expected<const Value *, Error> get_at(const Value &value, const Path &path) noexcept
{
	return walk_from(&value, path, 0);
}

std::expected<const Value *, Error> get_at(const Compound &compound, const Path &path) noexcept
{
	// Earlier versions wrapped `compound` in a temporary Value to reuse
	// the Value-based walker; that made the returned pointer dangle into
	// a stack temporary. Walk the first segment manually against the
	// borrowed Compound, then resume the loop on the located child.
	if (path.empty()) {
		return std::unexpected(wrong_type_error(0, "empty path on Compound"));
	}
	const auto &first = path.segments()[0];
	if (first.kind != PathSegment::Kind::Key) {
		return std::unexpected(wrong_type_error(0, "Compound walk must start with a Key segment"));
	}
	const Value *cur = find(compound, first.key);
	if (!cur) {
		return std::unexpected(not_found_error(0, "key not found"));
	}
	return walk_from(cur, path, 1);
}

std::expected<const Value *, Error> get_at(const Value &value, std::string_view path)
{
	auto parsed = parse_path(path);
	if (!parsed) {
		return std::unexpected(parsed.error());
	}
	return get_at(value, *parsed);
}

std::expected<const Value *, Error> get_at(const Compound &compound, std::string_view path)
{
	auto parsed = parse_path(path);
	if (!parsed) {
		return std::unexpected(parsed.error());
	}
	return get_at(compound, *parsed);
}

namespace {

template<class ArmT> [[nodiscard]] std::expected<ArmT, Error> get_arm_at(const Value &root, std::string_view path)
{
	auto leaf = get_at(root, path);
	if (!leaf) {
		return std::unexpected(leaf.error());
	}
	if (const ArmT *arm = std::get_if<ArmT>(&(*leaf)->v)) {
		return *arm;
	}
	return std::unexpected(make_error(ErrorCode::UnexpectedRootType, 0, "leaf has wrong arm"));
}

} // namespace

std::expected<std::int8_t, Error> get_byte_at(const Value &v, std::string_view p)
{
	return get_arm_at<std::int8_t>(v, p);
}
std::expected<std::int16_t, Error> get_short_at(const Value &v, std::string_view p)
{
	return get_arm_at<std::int16_t>(v, p);
}
std::expected<std::int32_t, Error> get_int_at(const Value &v, std::string_view p)
{
	return get_arm_at<std::int32_t>(v, p);
}
std::expected<std::int64_t, Error> get_long_at(const Value &v, std::string_view p)
{
	return get_arm_at<std::int64_t>(v, p);
}
std::expected<float, Error> get_float_at(const Value &v, std::string_view p)
{
	return get_arm_at<float>(v, p);
}
std::expected<double, Error> get_double_at(const Value &v, std::string_view p)
{
	return get_arm_at<double>(v, p);
}

std::expected<std::string_view, Error> get_string_at(const Value &v, std::string_view p)
{
	auto leaf = get_at(v, p);
	if (!leaf) {
		return std::unexpected(leaf.error());
	}
	if (const std::string *s = std::get_if<std::string>(&(*leaf)->v)) {
		return std::string_view{*s};
	}
	return std::unexpected(make_error(ErrorCode::UnexpectedRootType, 0, "leaf has wrong arm"));
}

} // namespace tagforge
