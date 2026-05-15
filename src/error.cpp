// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "tagforge/error.hpp"

namespace tagforge {

std::string_view Error::message() const noexcept
{
	if (!detail.empty()) {
		return detail;
	}
	return error_code_name(code);
}

std::string_view error_code_name(ErrorCode c) noexcept
{
	switch (c) {
	case ErrorCode::Ok:
		return "Ok";
	case ErrorCode::UnexpectedEndOfInput:
		return "UnexpectedEndOfInput";
	case ErrorCode::UnknownTagId:
		return "UnknownTagId";
	case ErrorCode::NegativeLength:
		return "NegativeLength";
	case ErrorCode::LengthOverflow:
		return "LengthOverflow";
	case ErrorCode::MixedListArm:
		return "MixedListArm";
	case ErrorCode::InvalidUtf8:
		return "InvalidUtf8";
	case ErrorCode::InvalidMutf8:
		return "InvalidMutf8";
	case ErrorCode::InvalidVarInt:
		return "InvalidVarInt";
	case ErrorCode::InvalidRoot:
		return "InvalidRoot";
	case ErrorCode::UnexpectedRootType:
		return "UnexpectedRootType";
	case ErrorCode::SnbtSyntax:
		return "SnbtSyntax";
	case ErrorCode::SnbtNumberOutOfRange:
		return "SnbtNumberOutOfRange";
	case ErrorCode::CompressionFailed:
		return "CompressionFailed";
	case ErrorCode::DecompressionFailed:
		return "DecompressionFailed";
	case ErrorCode::UnknownCodec:
		return "UnknownCodec";
	case ErrorCode::RegionInvalidHeader:
		return "RegionInvalidHeader";
	case ErrorCode::RegionInvalidChunk:
		return "RegionInvalidChunk";
	case ErrorCode::Io:
		return "Io";
	case ErrorCode::LimitExceeded:
		return "LimitExceeded";
	case ErrorCode::ViewRequiresMaterialise:
		return "ViewRequiresMaterialise";
	}
	return "UnknownErrorCode";
}

} // namespace tagforge
