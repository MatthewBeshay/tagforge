// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Internal limits guarding against deeply-nested compound/list trees that
// would otherwise blow the call stack. The cap is far above any real-world
// NBT structure (Minecraft's deepest published trees are < 20 levels deep)
// but comfortably below what a typical thread stack tolerates.

#pragma once

#include "tagforge/error.hpp"

namespace tagforge::detail {

inline constexpr int kMaxDecodeDepth = 512;

[[nodiscard]] inline Error depth_exceeded(std::size_t offset) noexcept
{
	return make_error(ErrorCode::LimitExceeded, offset, "nesting depth exceeds limit");
}

} // namespace tagforge::detail
