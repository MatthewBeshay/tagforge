// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace tagforge {

// Encode an owning tree into the given format. Returns the byte buffer
// allocated by tagforge; the caller takes ownership.
[[nodiscard]] std::expected<std::vector<std::byte>, Error> encode(const NamedValue &nv, Format format);
[[nodiscard]] std::expected<std::vector<std::byte>, Error> encode_anonymous(const Value &v, Format format);

// Encode into a caller-supplied output span; returns the number of bytes
// written. Useful for zero-allocation patches.
[[nodiscard]] std::expected<std::size_t, Error> encode_into(std::span<std::byte> out, const NamedValue &nv,
							    Format format);
[[nodiscard]] std::expected<std::size_t, Error> encode_anonymous_into(std::span<std::byte> out, const Value &v,
								      Format format);

// Streaming encoder. Appends successive NamedValue / Value roots into a
// caller-owned `std::vector<std::byte>`; the encoder borrows the sink and
// does not assume ownership of it. The sink must outlive the Encoder.
//
// Useful when packaging multiple NBT documents into a single buffer (e.g.
// chunk-batch payloads) without intermediate copies.
class Encoder {
public:
	Encoder(Format f, std::vector<std::byte> &sink) noexcept : format_{f}, sink_{&sink} {}

	[[nodiscard]] std::expected<void, Error> write(const NamedValue &nv);
	[[nodiscard]] std::expected<void, Error> write_anonymous(const Value &v);

private:
	Format format_;
	std::vector<std::byte> *sink_;
};

} // namespace tagforge
