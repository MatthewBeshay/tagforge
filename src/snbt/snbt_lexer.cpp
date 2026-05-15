// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// SNBT lexer is intentionally inlined into snbt_parser.cpp - the lexer is
// small and tightly coupled to the recursive-descent parser's lookahead.
// This TU exists only so CMakeLists.txt can list it for symmetry with
// `snbt_parser.cpp` and `snbt_writer.cpp`.
