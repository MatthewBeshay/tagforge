// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Dialect-agnostic glue for tagforge::View: `View::decode*`,
// `CompoundView::find`, `CompoundIterator`, `ListView::at`,
// `View::materialise`. Per-dialect wire-walk helpers live in
// view_<format>.cpp.

#include "tagforge/view.hpp"

#include "../internal/varint.hpp"
#include "../internal/view_internal.hpp"
#include "tagforge/cursor.hpp"
#include "tagforge/decode.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/mutf8.hpp"
#include "tagforge/tag_id.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tagforge {

// -- internal dispatch helpers ----------------------------------------------

namespace detail {

std::expected<std::size_t, Error> view_payload_size(Format f, std::span<const std::byte> bytes, std::size_t offset,
						    TagId tag)
{
	switch (f) {
	case Format::JavaNamedRoot:
	case Format::JavaAnonymousRoot:
		return java_payload_size(bytes, offset, tag);
	case Format::BedrockLittleEndian:
		return bedrock_le_payload_size(bytes, offset, tag);
	case Format::BedrockVarInt:
		return bedrock_varint_payload_size(bytes, offset, tag);
	}
	return std::unexpected(make_error(ErrorCode::InvalidRoot, offset, "unknown format"));
}

std::expected<ChildRecord, Error> view_read_child(Format f, std::span<const std::byte> bytes, std::size_t offset)
{
	switch (f) {
	case Format::JavaNamedRoot:
	case Format::JavaAnonymousRoot:
		return java_read_child(bytes, offset);
	case Format::BedrockLittleEndian:
		return bedrock_le_read_child(bytes, offset);
	case Format::BedrockVarInt:
		return bedrock_varint_read_child(bytes, offset);
	}
	return std::unexpected(make_error(ErrorCode::InvalidRoot, offset, "unknown format"));
}

} // namespace detail

// -- View ------------------------------------------------------------------

namespace {

[[nodiscard]] std::string_view ascii_view(std::span<const std::byte> bytes)
{
	return std::string_view{reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

[[nodiscard]] std::expected<View, Error> make_view_root(std::span<const std::byte> bytes, Format format,
							bool named_root)
{
	if (bytes.empty()) {
		return std::unexpected(make_error(ErrorCode::UnexpectedEndOfInput, 0));
	}
	Cursor c{bytes, 0};
	auto raw = c.read_int_be<std::uint8_t>();
	if (!raw) {
		return std::unexpected(raw.error());
	}
	if (!tag_id_is_valid(*raw)) {
		return std::unexpected(make_error(ErrorCode::UnknownTagId, 0));
	}
	const TagId tag = static_cast<TagId>(*raw);

	View v;
	v.bytes_ = bytes;
	v.format_ = format;
	v.tag_ = tag;

	if (tag == TagId::End) {
		v.payload_offset_ = c.pos; // points just past the 0x00
		v.name_view_ = {};
		return v;
	}

	if (named_root) {
		std::size_t name_len = 0;
		if (format == Format::BedrockVarInt) {
			auto vlen = detail::read_uvarint32(c);
			if (!vlen) {
				return std::unexpected(vlen.error());
			}
			name_len = *vlen;
		} else if (format == Format::BedrockLittleEndian) {
			auto llen = c.read_int_le<std::uint16_t>();
			if (!llen) {
				return std::unexpected(llen.error());
			}
			name_len = *llen;
		} else {
			auto blen = c.read_int_be<std::uint16_t>();
			if (!blen) {
				return std::unexpected(blen.error());
			}
			name_len = *blen;
		}
		if (auto e = c.need(name_len); !e) {
			return std::unexpected(e.error());
		}
		std::span<const std::byte> name_bytes{bytes.data() + c.pos, name_len};
		c.skip(name_len);
		if (is_pure_ascii(name_bytes)) {
			v.name_view_ = ascii_view(name_bytes);
		} else if (is_big_endian(format)) {
			// Java dialects store names as MUTF-8; non-ASCII names
			// need transcoding into the View's owned-string buffer.
			auto decoded = mutf8_to_utf8(name_bytes);
			if (!decoded) {
				return std::unexpected(decoded.error());
			}
			v.name_owned_ = std::move(*decoded);
			v.name_view_ = v.name_owned_;
		} else {
			// Bedrock names are plain UTF-8: zero-copy view straight
			// into the source buffer (matches the ASCII fast path).
			v.name_view_ = ascii_view(name_bytes);
		}
	} else {
		v.name_view_ = {};
	}
	v.payload_offset_ = c.pos;
	return v;
}

} // namespace

std::expected<View, Error> View::decode(std::span<const std::byte> bytes, Format format)
{
	return make_view_root(bytes, format, /*named_root=*/has_root_name(format));
}

std::expected<View, Error> View::decode_anonymous(std::span<const std::byte> bytes, Format format)
{
	return make_view_root(bytes, format, /*named_root=*/false);
}

std::string_view View::name_utf8() const noexcept
{
	return name_view_;
}

std::expected<std::int8_t, Error> View::as_byte() const noexcept
{
	if (tag_ != TagId::Byte) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	Cursor c{bytes_, payload_offset_};
	return c.read_int_be<std::int8_t>();
}

std::expected<std::int16_t, Error> View::as_short() const noexcept
{
	if (tag_ != TagId::Short) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	Cursor c{bytes_, payload_offset_};
	if (is_big_endian(format_)) {
		return c.read_int_be<std::int16_t>();
	}
	return c.read_int_le<std::int16_t>();
}

std::expected<std::int32_t, Error> View::as_int() const noexcept
{
	if (tag_ != TagId::Int) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	Cursor c{bytes_, payload_offset_};
	if (format_ == Format::BedrockVarInt) {
		return detail::read_zigzag32(c);
	}
	if (is_big_endian(format_)) {
		return c.read_int_be<std::int32_t>();
	}
	return c.read_int_le<std::int32_t>();
}

std::expected<std::int64_t, Error> View::as_long() const noexcept
{
	if (tag_ != TagId::Long) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	Cursor c{bytes_, payload_offset_};
	if (format_ == Format::BedrockVarInt) {
		return detail::read_zigzag64(c);
	}
	if (is_big_endian(format_)) {
		return c.read_int_be<std::int64_t>();
	}
	return c.read_int_le<std::int64_t>();
}

std::expected<float, Error> View::as_float() const noexcept
{
	if (tag_ != TagId::Float) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	Cursor c{bytes_, payload_offset_};
	return is_big_endian(format_) ? c.read_float_be() : c.read_float_le();
}

std::expected<double, Error> View::as_double() const noexcept
{
	if (tag_ != TagId::Double) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	Cursor c{bytes_, payload_offset_};
	return is_big_endian(format_) ? c.read_double_be() : c.read_double_le();
}

std::expected<std::string_view, Error> View::as_string() const noexcept
{
	if (tag_ != TagId::String) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	Cursor c{bytes_, payload_offset_};
	std::size_t len = 0;
	if (format_ == Format::BedrockVarInt) {
		auto vlen = detail::read_uvarint32(c);
		if (!vlen) {
			return std::unexpected(vlen.error());
		}
		len = *vlen;
	} else if (format_ == Format::BedrockLittleEndian) {
		auto llen = c.read_int_le<std::uint16_t>();
		if (!llen) {
			return std::unexpected(llen.error());
		}
		len = *llen;
	} else {
		auto blen = c.read_int_be<std::uint16_t>();
		if (!blen) {
			return std::unexpected(blen.error());
		}
		len = *blen;
	}
	if (auto e = c.need(len); !e) {
		return std::unexpected(e.error());
	}
	std::span<const std::byte> sbytes{bytes_.data() + c.pos, len};
	// Only the ASCII fast path is a true zero-copy view; non-ASCII Java
	// strings need MUTF-8 transcoding via materialise(). Bedrock strings
	// are plain UTF-8 and could be exposed as-is, but we keep the ASCII-
	// fast / materialise-otherwise contract uniform across dialects.
	if (is_pure_ascii(sbytes)) {
		return ascii_view(sbytes);
	}
	return std::unexpected(
		make_error(ErrorCode::ViewRequiresMaterialise, c.pos, "non-ASCII string requires materialise()"));
}

std::expected<CompoundView, Error> View::as_compound() const noexcept
{
	if (tag_ != TagId::Compound) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	CompoundView cv;
	cv.bytes_ = bytes_;
	cv.start_ = payload_offset_;
	cv.format_ = format_;
	return cv;
}

std::expected<ListView, Error> View::as_list() const noexcept
{
	if (tag_ != TagId::List) {
		return std::unexpected(make_error(ErrorCode::UnexpectedRootType, payload_offset_));
	}
	Cursor c{bytes_, payload_offset_};
	auto child = c.read_int_be<std::uint8_t>();
	if (!child) {
		return std::unexpected(child.error());
	}
	if (!tag_id_is_valid(*child)) {
		return std::unexpected(make_error(ErrorCode::UnknownTagId, c.pos - 1));
	}
	std::int32_t count = 0;
	if (format_ == Format::BedrockVarInt) {
		auto v = detail::read_zigzag32(c);
		if (!v) {
			return std::unexpected(v.error());
		}
		count = *v;
	} else if (format_ == Format::BedrockLittleEndian) {
		auto v = c.read_int_le<std::int32_t>();
		if (!v) {
			return std::unexpected(v.error());
		}
		count = *v;
	} else {
		auto v = c.read_int_be<std::int32_t>();
		if (!v) {
			return std::unexpected(v.error());
		}
		count = *v;
	}
	if (count < 0) {
		return std::unexpected(make_error(ErrorCode::NegativeLength, c.pos));
	}
	ListView lv;
	lv.bytes_ = bytes_;
	lv.start_ = c.pos;
	lv.format_ = format_;
	lv.arm_ = static_cast<TagId>(*child);
	lv.size_ = (lv.arm_ == TagId::End) ? 0 : static_cast<std::size_t>(count);
	return lv;
}

std::expected<Value, Error> View::materialise() const
{
	if (tag_ == TagId::End) {
		return Value{.v = EndTag{}};
	}
	if (payload_offset_ == 0) {
		return std::unexpected(make_error(ErrorCode::InvalidRoot, 0));
	}
	// Synthesise a fresh wire-form prefix in front of the payload bytes so
	// the existing decoder can consume the subtree. For Java we emit the
	// anonymous-root form (tag byte + payload); for Bedrock we emit a
	// tag byte + zero-length name + payload (the dialect's named-root
	// form with an empty name).
	std::vector<std::byte> buf;
	buf.reserve(bytes_.size() - payload_offset_ + 4);
	buf.push_back(static_cast<std::byte>(tag_));
	if (is_big_endian(format_)) {
		buf.insert(buf.end(), bytes_.data() + payload_offset_, bytes_.data() + bytes_.size());
		return tagforge::decode_anonymous(buf, Format::JavaAnonymousRoot);
	}
	if (format_ == Format::BedrockLittleEndian) {
		// uint16 LE name length = 0
		buf.push_back(std::byte{0});
		buf.push_back(std::byte{0});
	} else {
		// BedrockVarInt: single-byte uvarint of 0
		buf.push_back(std::byte{0});
	}
	buf.insert(buf.end(), bytes_.data() + payload_offset_, bytes_.data() + bytes_.size());
	auto nv = tagforge::decode(buf, format_);
	if (!nv) {
		return std::unexpected(nv.error());
	}
	return std::move(nv->value);
}

// -- CompoundView ----------------------------------------------------------

CompoundIterator::CompoundIterator(const CompoundView *parent, std::size_t offset) : parent_{parent}, offset_{offset}
{
	advance_to_next();
}

CompoundIterator &CompoundIterator::operator++()
{
	offset_ = next_offset_;
	advance_to_next();
	return *this;
}

void CompoundIterator::advance_to_next()
{
	if (parent_ == nullptr) {
		done_ = true;
		return;
	}
	auto rec = detail::view_read_child(parent_->format_, parent_->bytes_, offset_);
	if (!rec) {
		done_ = true;
		return;
	}
	if (rec->tag == TagId::End) {
		done_ = true;
		return;
	}
	std::span<const std::byte> name_bytes{parent_->bytes_.data() + rec->name_offset, rec->name_length};
	if (is_pure_ascii(name_bytes)) {
		current_.name.assign(reinterpret_cast<const char *>(name_bytes.data()), rec->name_length);
	} else {
		auto utf8 = mutf8_to_utf8(name_bytes);
		if (!utf8) {
			done_ = true;
			return;
		}
		current_.name = std::move(*utf8);
	}
	current_.child.bytes_ = parent_->bytes_;
	current_.child.format_ = parent_->format_;
	current_.child.tag_ = rec->tag;
	current_.child.payload_offset_ = rec->payload_offset;
	current_.child.name_owned_.clear();
	current_.child.name_view_ = {};
	next_offset_ = rec->payload_offset + rec->payload_size;
	done_ = false;
}

std::optional<View> CompoundView::find(std::string_view utf8_key) const
{
	std::size_t offset = start_;
	const bool key_is_ascii = is_pure_ascii(utf8_key);
	while (true) {
		auto rec = detail::view_read_child(format_, bytes_, offset);
		if (!rec) {
			return std::nullopt;
		}
		if (rec->tag == TagId::End) {
			return std::nullopt;
		}
		std::span<const std::byte> name_bytes{bytes_.data() + rec->name_offset, rec->name_length};
		bool match = false;
		if (key_is_ascii && is_pure_ascii(name_bytes)) {
			match = name_bytes.size() == utf8_key.size() &&
				std::memcmp(name_bytes.data(), utf8_key.data(), utf8_key.size()) == 0;
		} else {
			auto decoded = mutf8_to_utf8(name_bytes);
			if (!decoded) {
				return std::nullopt;
			}
			match = *decoded == utf8_key;
		}
		if (match) {
			View v;
			v.bytes_ = bytes_;
			v.format_ = format_;
			v.tag_ = rec->tag;
			v.payload_offset_ = rec->payload_offset;
			v.name_view_ = {};
			return v;
		}
		offset = rec->payload_offset + rec->payload_size;
	}
}

// -- ListView --------------------------------------------------------------

std::expected<View, Error> ListView::at(std::size_t i) const
{
	if (i >= size_) {
		return std::unexpected(make_error(ErrorCode::LengthOverflow, start_, "list index out of range"));
	}
	std::size_t offset = start_;
	for (std::size_t k = 0; k < i; ++k) {
		auto sz = detail::view_payload_size(format_, bytes_, offset, arm_);
		if (!sz) {
			return std::unexpected(sz.error());
		}
		offset += *sz;
	}
	View v;
	v.bytes_ = bytes_;
	v.format_ = format_;
	v.tag_ = arm_;
	v.payload_offset_ = offset;
	v.name_view_ = {};
	return v;
}

} // namespace tagforge
