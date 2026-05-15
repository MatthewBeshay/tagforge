// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Per-dialect encode entry points consumed by src/encode/encode_dispatch.cpp.

#pragma once

#include "tagforge/error.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <expected>
#include <vector>

namespace tagforge::detail {

[[nodiscard]] std::expected<void, Error> encode_java_named(const NamedValue &nv, std::vector<std::byte> &out);
[[nodiscard]] std::expected<void, Error> encode_java_anonymous(const Value &v, std::vector<std::byte> &out);
[[nodiscard]] std::expected<void, Error> encode_bedrock_le(const NamedValue &nv, std::vector<std::byte> &out);
[[nodiscard]] std::expected<void, Error> encode_bedrock_varint(const NamedValue &nv, std::vector<std::byte> &out);

} // namespace tagforge::detail
