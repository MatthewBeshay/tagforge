// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#pragma once

#include "tagforge/io.hpp"

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace bench {

inline std::filesystem::path fixture_dir()
{
	return std::filesystem::path{TAGFORGE_FIXTURE_DIR};
}

inline std::vector<std::byte> load(const std::filesystem::path &p)
{
	auto bytes = tagforge::read_file(p);
	if (!bytes) {
		throw std::runtime_error{"missing fixture: " + p.string()};
	}
	return std::move(*bytes);
}

inline const std::vector<std::byte> &bigtest_raw()
{
	static const auto bytes = load(fixture_dir() / "java" / "bigtest_raw.nbt");
	return bytes;
}

inline const std::vector<std::byte> &bigtest_gzip()
{
	static const auto bytes = load(fixture_dir() / "java" / "bigtest_gzip.nbt");
	return bytes;
}

} // namespace bench
