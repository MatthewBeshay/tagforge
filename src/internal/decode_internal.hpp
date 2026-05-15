// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Per-dialect decode entry points consumed by src/decode/decode_dispatch.cpp.

#pragma once

#include "tagforge/cursor.hpp"
#include "tagforge/error.hpp"
#include "tagforge/value.hpp"

#include <expected>

namespace tagforge::detail {

[[nodiscard]] std::expected<NamedValue, Error> decode_java_named(Cursor &c);
[[nodiscard]] std::expected<Value, Error> decode_java_anonymous(Cursor &c);
[[nodiscard]] std::expected<NamedValue, Error> decode_bedrock_le(Cursor &c);
[[nodiscard]] std::expected<NamedValue, Error> decode_bedrock_varint(Cursor &c);

} // namespace tagforge::detail
