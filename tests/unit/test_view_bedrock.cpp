// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/encode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/value.hpp"
#include "tagforge/view.hpp"

#include <cstdint>
#include <string>

using tagforge::Compound;
using tagforge::Format;
using tagforge::List;
using tagforge::NamedValue;
using tagforge::TagId;
using tagforge::Value;
using tagforge::View;

namespace {

NamedValue sample()
{
	Compound c;
	tagforge::upsert(c, "i", Value{.v = static_cast<std::int32_t>(123)});
	tagforge::upsert(c, "l", Value{.v = static_cast<std::int64_t>(-99999999LL)});
	tagforge::upsert(c, "f", Value{.v = 2.5f});
	tagforge::upsert(c, "s", Value{.v = std::string{"hi"}});
	Value list{.v = List{}};
	std::get<List>(list.v).push_back(Value{.v = static_cast<std::int32_t>(10)});
	std::get<List>(list.v).push_back(Value{.v = static_cast<std::int32_t>(20)});
	tagforge::upsert(c, "xs", std::move(list));
	return NamedValue{.name = "World", .value = Value{.v = std::move(c)}};
}

} // namespace

TEST_CASE("View decodes the root name of a Bedrock LE buffer", "[view][bedrock][le]")
{
	auto bytes = tagforge::encode(sample(), Format::BedrockLittleEndian);
	REQUIRE(bytes.has_value());
	auto root = View::decode(*bytes, Format::BedrockLittleEndian);
	REQUIRE(root.has_value());
	REQUIRE(root->kind() == TagId::Compound);
	REQUIRE(root->name_utf8() == "World");
}

TEST_CASE("CompoundView::find works against Bedrock LE", "[view][bedrock][le]")
{
	auto bytes = tagforge::encode(sample(), Format::BedrockLittleEndian);
	REQUIRE(bytes.has_value());
	auto root = View::decode(*bytes, Format::BedrockLittleEndian);
	auto cv = root->as_compound();
	REQUIRE(cv.has_value());

	auto i = cv->find("i");
	REQUIRE(i.has_value());
	REQUIRE(i->kind() == TagId::Int);
	REQUIRE(*i->as_int() == 123);

	auto l = cv->find("l");
	REQUIRE(l.has_value());
	REQUIRE(*l->as_long() == -99999999LL);

	auto s = cv->find("s");
	REQUIRE(s.has_value());
	REQUIRE(*s->as_string() == "hi");

	auto xs = cv->find("xs");
	REQUIRE(xs.has_value());
	auto list = xs->as_list();
	REQUIRE(list.has_value());
	REQUIRE(list->arm() == TagId::Int);
	REQUIRE(list->size() == 2);
	REQUIRE(*list->at(0)->as_int() == 10);
	REQUIRE(*list->at(1)->as_int() == 20);
}

TEST_CASE("View decodes the root name of a Bedrock VarInt buffer", "[view][bedrock][varint]")
{
	auto bytes = tagforge::encode(sample(), Format::BedrockVarInt);
	REQUIRE(bytes.has_value());
	auto root = View::decode(*bytes, Format::BedrockVarInt);
	REQUIRE(root.has_value());
	REQUIRE(root->kind() == TagId::Compound);
	REQUIRE(root->name_utf8() == "World");
}

TEST_CASE("CompoundView::find works against Bedrock VarInt (ZigZag ints + VarInt lengths)", "[view][bedrock][varint]")
{
	auto bytes = tagforge::encode(sample(), Format::BedrockVarInt);
	REQUIRE(bytes.has_value());
	auto root = View::decode(*bytes, Format::BedrockVarInt);
	auto cv = root->as_compound();
	REQUIRE(cv.has_value());

	REQUIRE(*cv->find("i")->as_int() == 123);
	REQUIRE(*cv->find("l")->as_long() == -99999999LL);
	REQUIRE(*cv->find("s")->as_string() == "hi");

	auto list = cv->find("xs")->as_list();
	REQUIRE(list.has_value());
	REQUIRE(list->size() == 2);
	REQUIRE(*list->at(1)->as_int() == 20);
}

TEST_CASE("View::materialise reconstructs Bedrock subtrees", "[view][bedrock]")
{
	for (auto fmt : {Format::BedrockLittleEndian, Format::BedrockVarInt}) {
		auto bytes = tagforge::encode(sample(), fmt);
		REQUIRE(bytes.has_value());
		auto root = View::decode(*bytes, fmt);
		auto cv = root->as_compound();
		auto leaf = cv->find("i");
		REQUIRE(leaf.has_value());
		auto v = leaf->materialise();
		REQUIRE(v.has_value());
		REQUIRE(v->kind() == TagId::Int);
		REQUIRE(std::get<std::int32_t>(v->v) == 123);
	}
}
