// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

// Umbrella header - re-exports every public-facing header. Translation units
// that only need a sub-feature should #include the narrower header directly.

#pragma once

#include "tagforge/compress.hpp"
#include "tagforge/cursor.hpp"
#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/io.hpp"
#include "tagforge/mutf8.hpp"
#include "tagforge/path.hpp"
#include "tagforge/region.hpp"
#include "tagforge/skip.hpp"
#include "tagforge/snbt.hpp"
#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"
#include "tagforge/view.hpp"
