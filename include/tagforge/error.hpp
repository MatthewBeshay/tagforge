// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tagforge {

// Closed enum of error codes. Every public function in tagforge that can fail
// returns std::expected<T, Error>; the Error carries the code, a byte offset
// (0 if not byte-positional), and an optional static-string detail.
enum class ErrorCode : std::uint16_t {
	Ok = 0,
	UnexpectedEndOfInput,
	// Wire-level: a tag-id byte on the buffer was not one of 0..12.
	// Path-level: `get_at` / `get_*_at` returns this when a compound key
	// along the requested path is missing - i.e. "no tag found at <key>".
	UnknownTagId,
	NegativeLength,
	LengthOverflow,
	MixedListArm,
	InvalidUtf8,
	InvalidMutf8,
	InvalidVarInt,
	InvalidRoot,
	// Wire-level: the top-level tag id did not match the dialect's expected
	// root (e.g. JavaNamedRoot expects a Compound).
	// Walk-level: `get_at` returns this when a path step lands on a Value
	// whose `kind()` is wrong for the next segment, and `get_*_at`
	// accessors return it when the leaf's variant arm does not match the
	// requested type.
	UnexpectedRootType,
	SnbtSyntax,
	SnbtNumberOutOfRange,
	CompressionFailed,
	DecompressionFailed,
	UnknownCodec,
	RegionInvalidHeader,
	RegionInvalidChunk,
	Io,
	LimitExceeded,
	// The caller asked a zero-copy `View` accessor for a value whose
	// representation cannot be returned as a borrowed `std::string_view`
	// - e.g. a non-ASCII Java string that needs MUTF-8 → UTF-8
	// transcoding. Use `View::materialise()` to obtain an owning Value.
	ViewRequiresMaterialise,
};

struct Error {
	ErrorCode code = ErrorCode::Ok;
	std::size_t offset = 0;
	std::string_view detail = {}; // pointer-to-static-string; never owned

	[[nodiscard]] std::string_view message() const noexcept;

	[[nodiscard]] friend bool operator==(const Error &, const Error &) noexcept = default;
};

[[nodiscard]] std::string_view error_code_name(ErrorCode) noexcept;

// Convenience constructor used internally. Public, but most callers use a
// returning function rather than constructing an Error directly.
[[nodiscard]] constexpr Error make_error(ErrorCode code, std::size_t offset = 0, std::string_view detail = {}) noexcept
{
	return Error{code, offset, detail};
}

} // namespace tagforge
