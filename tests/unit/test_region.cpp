// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include <catch2/catch_test_macros.hpp>

#include "tagforge/compress.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/region.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

using tagforge::Codec;
using tagforge::Compound;
using tagforge::ErrorCode;
using tagforge::Format;
using tagforge::NamedValue;
using tagforge::Region;
using tagforge::Value;

namespace {

NamedValue sample_chunk()
{
	Compound c;
	tagforge::upsert(c, "xPos", Value{.v = static_cast<std::int32_t>(1)});
	tagforge::upsert(c, "zPos", Value{.v = static_cast<std::int32_t>(2)});
	tagforge::upsert(c, "Status", Value{.v = std::string{"minecraft:full"}});
	return NamedValue{.name = "Level", .value = Value{.v = std::move(c)}};
}

// Assemble a region file by writing a single chunk via the public API and
// serialising. Used by the round-trip tests below.
std::vector<std::byte> assemble_region(int cx, int cz, std::span<const std::byte> chunk_named_root_nbt, Codec codec,
				       std::uint32_t timestamp_unix)
{
	Region r = Region::create();
	REQUIRE(r.write_chunk_bytes(cx, cz, chunk_named_root_nbt, codec, timestamp_unix).has_value());
	auto bytes = r.save_to_bytes();
	REQUIRE(bytes.has_value());
	return std::move(*bytes);
}

} // namespace

TEST_CASE("Region::open_from_bytes fails on a truncated header", "[region]")
{
	std::vector<std::byte> tiny(1024, std::byte{0});
	auto r = Region::open_from_bytes(std::move(tiny));
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error().code == ErrorCode::RegionInvalidHeader);
}

TEST_CASE("Synthesised region round-trips chunk(zlib)", "[region]")
{
	auto nv = sample_chunk();
	auto enc = tagforge::encode(nv, Format::JavaNamedRoot);
	REQUIRE(enc.has_value());

	auto region_bytes = assemble_region(
		/*cx=*/3, /*cz=*/5, std::span<const std::byte>{*enc}, Codec::Zlib,
		/*timestamp_unix=*/0xCAFE'BABEU);

	auto region = Region::open_from_bytes(std::move(region_bytes));
	REQUIRE(region.has_value());

	REQUIRE(region->has_chunk(3, 5));
	REQUIRE_FALSE(region->has_chunk(0, 0));
	REQUIRE(region->chunk_timestamp(3, 5) == 0xCAFE'BABEU);

	auto raw = region->chunk_raw(3, 5);
	REQUIRE(raw.has_value());
	REQUIRE(*raw == *enc);

	auto val = region->chunk_value(3, 5);
	REQUIRE(val.has_value());
	REQUIRE(val->name == "Level");
}

TEST_CASE("Synthesised region round-trips chunk(gzip)", "[region]")
{
	auto nv = sample_chunk();
	auto enc = tagforge::encode(nv, Format::JavaNamedRoot);
	REQUIRE(enc.has_value());

	auto region_bytes = assemble_region(10, 11, std::span<const std::byte>{*enc}, Codec::Gzip, 0);
	auto region = Region::open_from_bytes(std::move(region_bytes));
	REQUIRE(region.has_value());

	auto val = region->chunk_value(10, 11);
	REQUIRE(val.has_value());
	REQUIRE(val->name == "Level");
	REQUIRE(*tagforge::get_int(std::get<Compound>(val->value.v), "xPos") == 1);
}

TEST_CASE("Synthesised region with uncompressed chunk works", "[region]")
{
	auto nv = sample_chunk();
	auto enc = tagforge::encode(nv, Format::JavaNamedRoot);
	REQUIRE(enc.has_value());

	auto region_bytes = assemble_region(0, 0, std::span<const std::byte>{*enc}, Codec::None, 1);
	auto region = Region::open_from_bytes(std::move(region_bytes));
	REQUIRE(region.has_value());

	auto raw = region->chunk_raw(0, 0);
	REQUIRE(raw.has_value());
	REQUIRE(*raw == *enc);
}

TEST_CASE("Region::chunk_raw fails for empty slots", "[region][errors]")
{
	std::vector<std::byte> empty_region(8192, std::byte{0});
	auto region = Region::open_from_bytes(std::move(empty_region));
	REQUIRE(region.has_value());
	auto r = region->chunk_raw(0, 0);
	REQUIRE_FALSE(r.has_value());
	REQUIRE(r.error().code == ErrorCode::RegionInvalidChunk);
}

TEST_CASE("Region::create + write_chunk + save_to_bytes round-trips", "[region][writer]")
{
	Region r = Region::create();
	REQUIRE_FALSE(r.has_chunk(2, 7));

	REQUIRE(r.write_chunk(2, 7, sample_chunk(), Codec::Zlib, /*ts=*/0xCAFE'F00DU).has_value());
	REQUIRE(r.has_chunk(2, 7));
	REQUIRE(r.chunk_timestamp(2, 7) == 0xCAFE'F00DU);

	auto val = r.chunk_value(2, 7);
	REQUIRE(val.has_value());
	REQUIRE(val->name == "Level");

	auto bytes = r.save_to_bytes();
	REQUIRE(bytes.has_value());

	auto reopened = Region::open_from_bytes(std::move(*bytes));
	REQUIRE(reopened.has_value());
	REQUIRE(reopened->has_chunk(2, 7));
	REQUIRE(reopened->chunk_timestamp(2, 7) == 0xCAFE'F00DU);
	auto reopened_val = reopened->chunk_value(2, 7);
	REQUIRE(reopened_val.has_value());
	REQUIRE(reopened_val->name == "Level");
}

TEST_CASE("Region::populated_chunks iterates writes in row-major order", "[region][writer]")
{
	Region r = Region::create();
	REQUIRE(r.populated_chunks().empty());

	REQUIRE(r.write_chunk(0, 0, sample_chunk(), Codec::Zlib, 1).has_value());
	REQUIRE(r.write_chunk(31, 31, sample_chunk(), Codec::Zlib, 2).has_value());
	REQUIRE(r.write_chunk(15, 7, sample_chunk(), Codec::None, 3).has_value());

	auto pop = r.populated_chunks();
	REQUIRE(pop.size() == 3);
	REQUIRE(pop[0] == std::pair{0, 0});
	REQUIRE(pop[1] == std::pair{15, 7});
	REQUIRE(pop[2] == std::pair{31, 31});
}

TEST_CASE("Region::remove_chunk drops a slot on save", "[region][writer]")
{
	Region r = Region::create();
	REQUIRE(r.write_chunk(4, 5, sample_chunk(), Codec::Zlib, 1).has_value());
	r.remove_chunk(4, 5);
	REQUIRE_FALSE(r.has_chunk(4, 5));

	auto bytes = r.save_to_bytes();
	REQUIRE(bytes.has_value());

	auto reopened = Region::open_from_bytes(std::move(*bytes));
	REQUIRE(reopened.has_value());
	REQUIRE_FALSE(reopened->has_chunk(4, 5));
}
