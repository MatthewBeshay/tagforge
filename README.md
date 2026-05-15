# tagforge

A modern C++23 NBT (Minecraft Named Binary Tag) library - fast skipping, zero-copy navigation, owning trees, SNBT, region files, all four wire formats, byte-exact round-trip.

## Contents

- [Quickstart](#quickstart)
- [Examples](#examples)
  - [Read an .nbt file (autodetected format + codec)](#read-an-nbt-file-autodetected-format--codec)
  - [Build a tree and write it back](#build-a-tree-and-write-it-back)
  - [Zero-copy navigation with `View`](#zero-copy-navigation-with-view)
  - [SNBT (Mojangson) round-trip](#snbt-mojangson-round-trip)
  - [NBT path queries](#nbt-path-queries)
  - [Region files (.mca)](#region-files-mca)
  - [Compression](#compression)
  - [Streaming](#streaming)
- [Wire formats](#wire-formats)
- [Build](#build)
- [Integration](#integration)
- [Benchmarks](#benchmarks)
- [License](#license)

## Quickstart

```cpp
#include <tagforge/io.hpp>
#include <tagforge/value.hpp>
#include <print>

int main() {
    auto nv = tagforge::read_nbt_file("level.dat"); // gzip/zlib + dialect autodetect
    if (!nv) return 1;

    const auto& data = std::get<tagforge::Compound>(nv->value.v);
    std::println("Level: {}", *tagforge::get_string(data, "LevelName"));
}
```

That's the whole "read a Minecraft world's `level.dat`" path. Five function calls, one allocation per string.

## Examples

### Read an .nbt file (autodetected format + codec)

`read_nbt_file` runs three layers of autodetection: codec (gzip / zlib / raw), then format (Java BE, Bedrock LE, Bedrock VarInt), then decode.

```cpp
#include <tagforge/io.hpp>
#include <tagforge/value.hpp>

auto nv = tagforge::read_nbt_file("bigtest.nbt.gz");
if (!nv) {
    std::println(stderr, "decode failed: {}", nv.error().message());
    return 1;
}
// nv->name           - root key (empty for Java anonymous root)
// nv->value.kind()   - TagId of the root payload
// nv->value.v        - std::variant holding the typed payload
```

If you already know the format and codec, use the lower-level entry points directly they take the same amount of code and skip the heuristic:

```cpp
auto bytes = tagforge::read_file("save.nbt").value();
auto raw = tagforge::decompress(bytes).value(); // codec autodetect
auto nv = tagforge::decode(raw, tagforge::Format::JavaNamedRoot).value();
```

### Build a tree and write it back

The owning tree is a `std::variant`-backed `Value` with free-function accessors. Compounds are `std::vector<std::pair<std::string, Value>>` fully transparent.

```cpp
#include <tagforge/encode.hpp>
#include <tagforge/io.hpp>
#include <tagforge/value.hpp>

using namespace tagforge;

Compound stats;
upsert(stats, "deaths", Value{.v = std::int32_t{3}});
upsert(stats, "playtime", Value{.v = std::int64_t{12'345}});
upsert(stats, "hardcore", Value{.v = std::int8_t{1}});

NamedValue root{.name = "Player", .value = Value{.v = std::move(stats)}};

auto written = write_nbt_file("player.dat", root,
                              Format::JavaNamedRoot,
                              Codec::Gzip);
```

`upsert` preserves insertion order, which is what makes round-trip byte-exact.

### Zero-copy navigation with `View`

`View::decode` records offsets into a borrowed byte span. Children resolve lazily perfect for iterating a region file looking for one key without materialising every chunk.

```cpp
#include <tagforge/view.hpp>

auto bytes = tagforge::read_file("chunk.nbt").value();
auto root = tagforge::View::decode(bytes, tagforge::Format::JavaNamedRoot).value();

auto level = root.as_compound().value();
if (auto sections = level.find("sections")) {
    auto list = sections->as_list().value();
    for (std::size_t i = 0; i < list.size(); ++i) {
        auto section = list.at(i).value();
        auto y = section.as_compound()->find("Y")->as_byte().value();
        std::println("section Y = {}", y);
    }
}
```

Only the field you call `as_*()` on is parsed. Non-ASCII Java strings (MUTF-8) need transcoding; for those, call `materialise()` to allocate an owning subtree.

### SNBT (Mojangson) round-trip

```cpp
#include <tagforge/snbt.hpp>

auto v = tagforge::parse_snbt(R"({Items:[{id:"minecraft:diamond",Count:64b}]})").value();

tagforge::SnbtOptions opt{.pretty = true, .indent_width = 2};
std::println("{}", tagforge::to_snbt(v, opt));
```

Parses what Mojang's `/data` command emits; emits something Mojang's `/data` command will accept.

### NBT path queries

The Mojang `/data`-style path syntax: dotted keys, bracketed indices, quoted keys for names that contain dots.

```cpp
#include <tagforge/path.hpp>

auto name = tagforge::get_string_at(player_value, "Inventory[0].tag.display.Name");
if (name) std::println("first slot: {}", *name);

// Quoted keys when a key contains a literal '.':
auto custom = tagforge::get_at(player_value, R"(["minecraft:custom_data"])");
```

Typed leaf accessors (`get_int_at`, `get_long_at`, `get_string_at`, ...) fail with `UnknownTagId` if the key is missing, `UnexpectedRootType` if the leaf has the wrong arm.

### Region files (.mca)

Read, mutate, and write Minecraft region files including timestamps and per-chunk codec choice.

```cpp
#include <tagforge/region.hpp>

auto region = tagforge::Region::open("r.0.0.mca").value();

for (auto [cx, cz] : region.populated_chunks()) {
    auto view = region.chunk_view(cx, cz).value(); // zero-copy
    auto status = view.as_compound()->find("Status")->as_string().value();
    std::println("chunk ({}, {}) → {}", cx, cz, status);
}

// Mutate: replace one chunk and save.
auto updated = make_chunk_tree(/* ... */);
region.write_chunk(0, 0, updated, tagforge::Codec::Zlib).value();
region.save("r.0.0.mca").value();
```

### Compression

```cpp
#include <tagforge/compress.hpp>

auto raw = tagforge::read_file("save.nbt.gz").value();
auto codec = tagforge::detect_codec(raw); // gzip / zlib / lz4 / nullopt
auto decompressed = tagforge::decompress(raw).value(); // autodetect
auto recompressed = tagforge::compress(decompressed,
                                       tagforge::Codec::Zlib,
                                       tagforge::compression_level::high).value();
```

LZ4 frame format is supported (the `.lz4` magic + the codec byte some 1.20.5+ region chunks use).

### Streaming

For packet pipelines that batch multiple NBT roots into one buffer:

```cpp
#include <tagforge/encode.hpp>
#include <tagforge/decode.hpp>
#include <tagforge/cursor.hpp>

// Producer side append two roots into one sink.
std::vector<std::byte> sink;
tagforge::Encoder enc{tagforge::Format::JavaNamedRoot, sink};
enc.write(root_a).value();
enc.write(root_b).value();

// Consumer side drive a Cursor through the concatenated buffer.
tagforge::Cursor cur{.data = sink};
tagforge::Decoder dec{tagforge::Format::JavaNamedRoot};
while (!cur.empty()) {
    auto nv = dec.decode(cur).value();
    handle(nv);
}
```

## Wire formats

All four dialects Minecraft uses, distinguished by an explicit `Format`:

| `Format`              | Endianness | Strings   | Lengths  | Used by                                 |
|-----------------------|------------|-----------|----------|-----------------------------------------|
| `JavaNamedRoot`       | big        | MUTF-8    | int32 BE | Java disk saves, region chunks (`.mca`) |
| `JavaAnonymousRoot`   | big        | MUTF-8    | int32 BE | Java 1.20.2+ network NBT                |
| `BedrockLittleEndian` | little     | UTF-8     | int32 LE | Bedrock disk saves (`level.dat`)        |
| `BedrockVarInt`       | little     | UTF-8     | ZigZag   | Bedrock network NBT                     |

`tagforge::detect_format` autodetects from the first few bytes for the named-root dialects.

## Build

Prerequisites: Visual Studio 2022 (or Build Tools) with the C++ workload, CMake 3.28+, [vcpkg](https://github.com/microsoft/vcpkg) on `VCPKG_ROOT`, Ninja.

```pwsh
$env:VCPKG_ROOT = "<path-to-vcpkg>" # set once; see https://github.com/microsoft/vcpkg
cmake --preset msvc
cmake --build --preset msvc-release
ctest --preset msvc -C Release
```

Available presets: `msvc`, `msvc-asan`, `vs2022`, `vs2026`. Set `-DTAGFORGE_BUILD_CLI=ON` (default on the Windows presets) to also build the `nbt-cli` debugging tool.

```pwsh
# nbt-cli round-trips a fixture, exits 0 if byte-exact:
build/msvc/tools/nbt-cli/Release/nbt-cli.exe verify --format java tests/fixtures/java/bigtest_raw.nbt
```

## Integration

Install once, then `find_package` from any downstream CMake project.

```pwsh
cmake --install build/msvc --prefix <prefix> --config Release
```

```cmake
find_package(tagforge CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE tagforge::tagforge)
```

`tagforge::tagforge_headers` is exported separately if you only need the header-only types (e.g. `Cursor`).

The whole library is one header away if you want it:

```cpp
#include <tagforge/tagforge.hpp> // umbrella; pulls in every public header
```

Prefer the narrower headers in production code (`tagforge/decode.hpp`, `tagforge/view.hpp`, ...) they compile faster and document intent.

## Benchmarks

MSVC 19.51 (VS 2026 Insiders), x64 Release, single thread (Ryzen 9 5950x), [Google Benchmark](https://github.com/google/benchmark) with `--benchmark_min_time=2s`.

| Library                                                                          |    Throughput |
|----------------------------------------------------------------------------------|--------------:|
| **[tagforge](https://github.com/matthewbeshay/tagforge)** *(skip, no alloc.)*    |    6.30 GiB/s |
| **[tagforge](https://github.com/matthewbeshay/tagforge)** *(decode, owning)*     |     392 MiB/s |
| [GlacieTeam/NBT](https://github.com/GlacieTeam/NBT)                              |     328 MiB/s |
| [Celisium/libnbt](https://github.com/Celisium/libnbt)                            |     326 MiB/s |
| [JaanDev/nbtpp](https://github.com/JaanDev/nbtpp)                                |     255 MiB/s |
| [maspitz/nbtview](https://github.com/maspitz/nbtview)                            |     220 MiB/s |
| [PrismLauncher/libnbtplusplus](https://github.com/PrismLauncher/libnbtplusplus)  |     206 MiB/s |
| [SpockBotMC/cpp-nbt](https://github.com/SpockBotMC/cpp-nbt)                      |      62 MiB/s |
| [handtruth/nbt-cpp](https://github.com/handtruth/nbt-cpp)                        |      45 MiB/s |
| [max-ishere/nbt-blacksmith](https://github.com/max-ishere/nbt-blacksmith)        |      39 MiB/s |
| [M4xi1m3/nbtpp](https://github.com/M4xi1m3/nbtpp)                                |      38 MiB/s |

tagforge wins every head-to-head ~20% over the next contender ([GlacieTeam/NBT](https://github.com/GlacieTeam/NBT)), ~30% over [Celisium/libnbt](https://github.com/Celisium/libnbt), ~7× over the slowest C++ contestants. Allocation-free `skip` sets the absolute ceiling at 6.30 GiB/s about 16× the owning-tree throughput, which is what a "give me an NBT tree I can mutate" workload pays for the heap structure.

Cross-library comparisons live in the separate [nbt_performance](https://github.com/matthewbeshay/nbt_performance) repo, which pulls tagforge and every competing NBT library above via `FetchContent` and runs them against the same workload.

## License

Copyright (c) 2026 Matthew Beshay.

tagforge is distributed under the MIT License. The full text is in
[LICENSE](LICENSE); a copy of the copyright notice and permission notice
must be included in all copies or substantial portions of the Software.

The software is provided "AS IS", without warranty of any kind, express or
implied.