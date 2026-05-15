// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/error.hpp"
#include "tagforge/value.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace tagforge {

// Options for `to_snbt`. Defaults match Mojang's `/data` command output.
struct SnbtOptions {
	bool pretty = false;
	std::uint8_t indent_width = 2;
	bool quote_all_keys = false;
	bool sort_keys = false;        // off: preserves Compound insertion order
	bool array_suffix_caps = true; // [B; ...]  [I; ...]  [L; ...]  per vanilla
};

[[nodiscard]] std::expected<Value, Error> parse_snbt(std::string_view text);
[[nodiscard]] std::string to_snbt(const Value &v, const SnbtOptions &opts = {});
[[nodiscard]] std::string to_snbt(const NamedValue &nv, const SnbtOptions &opts = {});

} // namespace tagforge
