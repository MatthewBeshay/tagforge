// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace tagforge {

class CompoundView;
class ListView;
class CompoundIterator;
struct CompoundEntry;

// A zero-copy lazy view over an NBT byte buffer.
class View {
public:
	[[nodiscard]] static std::expected<View, Error> decode(std::span<const std::byte> bytes, Format format);
	[[nodiscard]] static std::expected<View, Error> decode_anonymous(std::span<const std::byte> bytes,
									 Format format);

	[[nodiscard]] TagId kind() const noexcept { return tag_; }
	[[nodiscard]] Format format() const noexcept { return format_; }

	[[nodiscard]] std::string_view name_utf8() const noexcept;

	[[nodiscard]] std::expected<std::int8_t, Error> as_byte() const noexcept;
	[[nodiscard]] std::expected<std::int16_t, Error> as_short() const noexcept;
	[[nodiscard]] std::expected<std::int32_t, Error> as_int() const noexcept;
	[[nodiscard]] std::expected<std::int64_t, Error> as_long() const noexcept;
	[[nodiscard]] std::expected<float, Error> as_float() const noexcept;
	[[nodiscard]] std::expected<double, Error> as_double() const noexcept;
	// Returns a `string_view` borrowed from the source buffer for ASCII
	// and (Bedrock) UTF-8 payloads. For non-ASCII Java strings call
	// `materialise()` instead - those need MUTF-8 → UTF-8 transcoding,
	// and this accessor returns `ErrorCode::ViewRequiresMaterialise`.
	[[nodiscard]] std::expected<std::string_view, Error> as_string() const noexcept;

	[[nodiscard]] std::expected<CompoundView, Error> as_compound() const noexcept;
	[[nodiscard]] std::expected<ListView, Error> as_list() const noexcept;

	// Allocates an owning Value subtree by synthesising a minimal wire
	// prefix (a tag byte for Java, a tag byte plus zero-length name for
	// Bedrock) and re-running the decoder. Cheap for small subtrees;
	// allocates only the destination tree, not an intermediate buffer
	// proportional to the source.
	[[nodiscard]] std::expected<Value, Error> materialise() const;

	// Public construction is allowed; the type holds no invariants we
	// need to defend beyond what its members already enforce. Internal
	// helpers (View::decode*, the iterator) populate the fields.
	std::span<const std::byte> bytes_{};
	std::size_t payload_offset_ = 0;
	Format format_ = Format::JavaNamedRoot;
	TagId tag_ = TagId::End;
	std::string name_owned_;
	mutable std::string_view name_view_;
};

struct CompoundEntry {
	std::string name;
	View child;
};

class CompoundIterator {
public:
	using value_type = CompoundEntry;
	using reference = const CompoundEntry &;
	using pointer = const CompoundEntry *;
	using difference_type = std::ptrdiff_t;
	using iterator_category = std::input_iterator_tag;

	CompoundIterator() = default;
	CompoundIterator(const CompoundView *parent, std::size_t offset);

	reference operator*() const noexcept { return current_; }
	pointer operator->() const noexcept { return &current_; }

	CompoundIterator &operator++();

	friend bool operator==(const CompoundIterator &a, const CompoundIterator &b) noexcept
	{
		// End-state iterators compare equal even if their parent_ pointers
		// differ (a default-constructed end() sentinel has parent_ == nullptr).
		if (a.done_ && b.done_) {
			return true;
		}
		if (a.done_ != b.done_) {
			return false;
		}
		return a.parent_ == b.parent_ && a.offset_ == b.offset_;
	}
	friend bool operator!=(const CompoundIterator &a, const CompoundIterator &b) noexcept { return !(a == b); }

private:
	void advance_to_next();

	const CompoundView *parent_ = nullptr;
	std::size_t offset_ = 0;
	std::size_t next_offset_ = 0;
	bool done_ = true;
	CompoundEntry current_{};
};

class CompoundView {
public:
	using iterator = CompoundIterator;

	[[nodiscard]] std::optional<View> find(std::string_view utf8_key) const;
	[[nodiscard]] iterator begin() const { return iterator{this, start_}; }
	[[nodiscard]] iterator end() const { return iterator{}; }

	// Public so View::as_compound() can populate without an extra friend
	// declaration; treat as internal.
	std::span<const std::byte> bytes_{};
	std::size_t start_ = 0;
	Format format_ = Format::JavaNamedRoot;
};

class ListView {
public:
	[[nodiscard]] TagId arm() const noexcept { return arm_; }
	[[nodiscard]] std::size_t size() const noexcept { return size_; }
	[[nodiscard]] std::expected<View, Error> at(std::size_t i) const;

	// Treat as internal.
	std::span<const std::byte> bytes_{};
	std::size_t start_ = 0;
	Format format_ = Format::JavaNamedRoot;
	TagId arm_ = TagId::End;
	std::size_t size_ = 0;
};

} // namespace tagforge
