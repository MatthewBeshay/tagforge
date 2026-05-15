// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/cursor.hpp"
#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/error.hpp"
#include "tagforge/format.hpp"
#include "tagforge/path.hpp"
#include "tagforge/region.hpp"
#include "tagforge/value.hpp"
#include "tagforge/view.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

using tagforge::Codec;
using tagforge::Compound;
using tagforge::Cursor;
using tagforge::Decoder;
using tagforge::Encoder;
using tagforge::ErrorCode;
using tagforge::Format;
using tagforge::NamedValue;
using tagforge::Region;
using tagforge::TagId;
using tagforge::Value;
using tagforge::View;

namespace {

NamedValue sample_named(std::string_view name, std::int32_t n)
{
	Compound c;
	tagforge::upsert(c, "n", Value{.v = n});
	return NamedValue{.name = std::string{name}, .value = Value{.v = std::move(c)}};
}

std::filesystem::path temp_path(std::string_view name)
{
	auto p = std::filesystem::temp_directory_path() / "tagforge_streaming";
	std::filesystem::create_directories(p);
	return p / name;
}

} // namespace

TEST_CASE("Encoder::write appends successive NamedValues into one sink", "[encode][stream]")
{
	std::vector<std::byte> sink;
	Encoder enc{Format::JavaNamedRoot, sink};

	REQUIRE(enc.write(sample_named("a", 1)).has_value());
	const auto split = sink.size();
	REQUIRE(enc.write(sample_named("b", 2)).has_value());
	REQUIRE(sink.size() > split);

	Cursor cur{.data = sink};
	Decoder dec{Format::JavaNamedRoot};

	auto first = dec.decode(cur);
	REQUIRE(first.has_value());
	REQUIRE(first->name == "a");
	REQUIRE(*tagforge::get_int(std::get<Compound>(first->value.v), "n") == 1);
	REQUIRE(cur.pos == split);

	auto second = dec.decode(cur);
	REQUIRE(second.has_value());
	REQUIRE(second->name == "b");
	REQUIRE(*tagforge::get_int(std::get<Compound>(second->value.v), "n") == 2);
	REQUIRE(cur.empty());
}

TEST_CASE("Encoder::write_anonymous appends two anonymous roots", "[encode][stream]")
{
	std::vector<std::byte> sink;
	Encoder enc{Format::JavaAnonymousRoot, sink};

	Value v1{.v = static_cast<std::int32_t>(7)};
	Value v2{.v = static_cast<std::int32_t>(8)};
	REQUIRE(enc.write_anonymous(v1).has_value());
	REQUIRE(enc.write_anonymous(v2).has_value());

	Cursor cur{.data = sink};
	Decoder dec{Format::JavaAnonymousRoot};

	auto first = dec.decode_anonymous(cur);
	REQUIRE(first.has_value());
	REQUIRE(first->kind() == TagId::Int);
	REQUIRE(std::get<std::int32_t>(first->v) == 7);

	auto second = dec.decode_anonymous(cur);
	REQUIRE(second.has_value());
	REQUIRE(std::get<std::int32_t>(second->v) == 8);
	REQUIRE(cur.empty());
}

TEST_CASE("encode_into writes into a right-sized buffer", "[encode][stream]")
{
	auto nv = sample_named("root", 42);
	auto reference = tagforge::encode(nv, Format::JavaNamedRoot);
	REQUIRE(reference.has_value());

	std::vector<std::byte> out(reference->size());
	auto n = tagforge::encode_into(std::span<std::byte>{out}, nv, Format::JavaNamedRoot);
	REQUIRE(n.has_value());
	REQUIRE(*n == reference->size());
	REQUIRE(out == *reference);
}

TEST_CASE("encode_into reports LengthOverflow when the destination is too small", "[encode][stream][errors]")
{
	auto nv = sample_named("root", 1);
	std::vector<std::byte> out(2);
	auto n = tagforge::encode_into(std::span<std::byte>{out}, nv, Format::JavaNamedRoot);
	REQUIRE_FALSE(n.has_value());
	REQUIRE(n.error().code == ErrorCode::LengthOverflow);
}

TEST_CASE("encode_anonymous_into round-trips through decode_anonymous", "[encode][stream]")
{
	Value v{.v = std::string{"hello"}};
	auto reference = tagforge::encode_anonymous(v, Format::JavaAnonymousRoot);
	REQUIRE(reference.has_value());

	std::vector<std::byte> out(reference->size());
	auto n = tagforge::encode_anonymous_into(std::span<std::byte>{out}, v, Format::JavaAnonymousRoot);
	REQUIRE(n.has_value());
	REQUIRE(*n == reference->size());

	auto round = tagforge::decode_anonymous(out, Format::JavaAnonymousRoot);
	REQUIRE(round.has_value());
	REQUIRE(std::get<std::string>(round->v) == "hello");
}

TEST_CASE("View::as_string returns ViewRequiresMaterialise for non-ASCII Java strings", "[view][errors]")
{
	Compound c;
	// Greek delta (U+0394) - outside ASCII, so a borrowed string_view
	// would require MUTF-8 transcoding.
	tagforge::upsert(c, "label", Value{.v = std::string{"\xCE\x94"}});
	NamedValue nv{.name = "root", .value = Value{.v = std::move(c)}};

	auto bytes = tagforge::encode(nv, Format::JavaNamedRoot);
	REQUIRE(bytes.has_value());

	auto view = View::decode(*bytes, Format::JavaNamedRoot);
	REQUIRE(view.has_value());
	auto compound = view->as_compound();
	REQUIRE(compound.has_value());
	auto child = compound->find("label");
	REQUIRE(child.has_value());

	auto s = child->as_string();
	REQUIRE_FALSE(s.has_value());
	REQUIRE(s.error().code == ErrorCode::ViewRequiresMaterialise);

	auto owned = child->materialise();
	REQUIRE(owned.has_value());
	REQUIRE(std::get<std::string>(owned->v) == "\xCE\x94");
}

TEST_CASE("View::materialise reconstructs a Bedrock VarInt subtree under a named root", "[view][bedrock][varint]")
{
	Compound inner;
	tagforge::upsert(inner, "i", Value{.v = static_cast<std::int32_t>(123)});
	tagforge::upsert(inner, "s", Value{.v = std::string{"abc"}});

	Compound outer;
	tagforge::upsert(outer, "child", Value{.v = std::move(inner)});

	NamedValue nv{.name = "container", .value = Value{.v = std::move(outer)}};

	auto bytes = tagforge::encode(nv, Format::BedrockVarInt);
	REQUIRE(bytes.has_value());

	auto view = View::decode(*bytes, Format::BedrockVarInt);
	REQUIRE(view.has_value());
	REQUIRE(view->name_utf8() == "container");

	auto outer_cv = view->as_compound();
	REQUIRE(outer_cv.has_value());
	auto child_view = outer_cv->find("child");
	REQUIRE(child_view.has_value());
	REQUIRE(child_view->kind() == TagId::Compound);

	auto owned = child_view->materialise();
	REQUIRE(owned.has_value());
	const auto &c = std::get<Compound>(owned->v);
	REQUIRE(*tagforge::get_int(c, "i") == 123);
	REQUIRE(*tagforge::get_string(c, "s") == "abc");
}

TEST_CASE("parse_path accepts quoted-key escapes", "[path]")
{
	// ["a\"b"] → key contains a literal double-quote.
	auto p1 = tagforge::parse_path(R"(["a\"b"])");
	REQUIRE(p1.has_value());
	REQUIRE(p1->size() == 1);
	REQUIRE(p1->segments()[0].key == "a\"b");

	// ["a\\b"] → key contains a literal backslash.
	auto p2 = tagforge::parse_path(R"(["a\\b"])");
	REQUIRE(p2.has_value());
	REQUIRE(p2->size() == 1);
	REQUIRE(p2->segments()[0].key == "a\\b");

	// Reject leading sign in numeric index: `Items[-1]` should fail cleanly.
	auto p3 = tagforge::parse_path("Items[-1]");
	REQUIRE_FALSE(p3.has_value());
	REQUIRE(p3.error().code == ErrorCode::SnbtSyntax);
}

TEST_CASE("Region::save writes a round-trippable region file", "[region][writer][io]")
{
	const auto path = temp_path("r.0.0.mca");
	std::filesystem::remove(path);

	Region r = Region::create();
	REQUIRE(r.write_chunk(2, 7, sample_named("Level", 99), Codec::Zlib, /*ts=*/123).has_value());
	REQUIRE(r.save(path).has_value());

	auto reopened = Region::open(path);
	REQUIRE(reopened.has_value());
	REQUIRE(reopened->has_chunk(2, 7));
	REQUIRE(reopened->chunk_timestamp(2, 7) == 123U);

	auto val = reopened->chunk_value(2, 7);
	REQUIRE(val.has_value());
	REQUIRE(val->name == "Level");
	REQUIRE(*tagforge::get_int(std::get<Compound>(val->value.v), "n") == 99);
}
