// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// nbt-cli - debug + verification tool for tagforge.

#include <CLI/CLI.hpp>

#include "tagforge/compress.hpp"
#include "tagforge/decode.hpp"
#include "tagforge/encode.hpp"
#include "tagforge/format.hpp"
#include "tagforge/region.hpp"
#include "tagforge/skip.hpp"
#include "tagforge/snbt.hpp"
#include "tagforge/value.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> slurp(const std::filesystem::path &p)
{
	std::ifstream f(p, std::ios::binary | std::ios::ate);
	if (!f.is_open()) {
		throw std::runtime_error("could not open " + p.string());
	}
	const std::streamsize n = f.tellg();
	f.seekg(0);
	std::vector<std::byte> out(static_cast<std::size_t>(n));
	f.read(reinterpret_cast<char *>(out.data()), n);
	return out;
}

void write_stdout(const std::vector<std::byte> &bytes)
{
	std::fwrite(bytes.data(), 1, bytes.size(), stdout);
}

tagforge::Format parse_format(const std::string &s, std::span<const std::byte> hint = {})
{
	if (s == "auto" || s.empty()) {
		auto detected = tagforge::detect_format(hint);
		if (!detected) {
			throw std::runtime_error("could not auto-detect format; pass --format explicitly");
		}
		return *detected;
	}
	if (s == "java") {
		return tagforge::Format::JavaNamedRoot;
	}
	if (s == "java-net") {
		return tagforge::Format::JavaAnonymousRoot;
	}
	if (s == "bedrock-le") {
		return tagforge::Format::BedrockLittleEndian;
	}
	if (s == "bedrock-varint") {
		return tagforge::Format::BedrockVarInt;
	}
	throw std::runtime_error("unknown format: " + s + " (auto | java | java-net | bedrock-le | bedrock-varint)");
}

[[nodiscard]] std::vector<std::byte> decompress_if_compressed(std::vector<std::byte> bytes)
{
	auto codec = tagforge::detect_codec(bytes);
	if (!codec) {
		return bytes;
	}
	auto out = tagforge::decompress(bytes);
	if (!out) {
		throw std::runtime_error("decompression failed: " + std::string{out.error().message()});
	}
	return *out;
}

int cmd_dump(const std::string &path, const std::string &fmt_name, bool pretty, int indent)
{
	const auto bytes = decompress_if_compressed(slurp(path));
	const auto fmt = parse_format(fmt_name, bytes);
	auto value = tagforge::decode(bytes, fmt);
	if (!value) {
		std::cerr << "decode failed: " << value.error().message() << '\n';
		return 1;
	}
	tagforge::SnbtOptions opts;
	opts.pretty = pretty;
	opts.indent_width = static_cast<std::uint8_t>(indent);
	std::cout << tagforge::to_snbt(value->value, opts) << '\n';
	return 0;
}

int cmd_to_snbt(const std::string &path, const std::string &fmt_name, bool pretty)
{
	return cmd_dump(path, fmt_name, pretty, /*indent=*/2);
}

int cmd_from_snbt(const std::string &path, const std::string &fmt_name)
{
	std::ifstream f(path);
	if (!f.is_open()) {
		std::cerr << "could not open " << path << '\n';
		return 1;
	}
	std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	auto parsed = tagforge::parse_snbt(text);
	if (!parsed) {
		std::cerr << "parse failed at offset " << parsed.error().offset << ": " << parsed.error().message()
			  << '\n';
		return 1;
	}
	const auto fmt = parse_format(fmt_name);
	auto enc = tagforge::encode_anonymous(*parsed, fmt);
	if (!enc) {
		std::cerr << "encode failed: " << enc.error().message() << '\n';
		return 1;
	}
	write_stdout(*enc);
	return 0;
}

int cmd_decompress(const std::string &path)
{
	const auto bytes = slurp(path);
	auto out = tagforge::decompress(bytes);
	if (!out) {
		std::cerr << "decompress failed: " << out.error().message() << '\n';
		return 1;
	}
	write_stdout(*out);
	return 0;
}

int cmd_region_list(const std::string &path)
{
	auto region = tagforge::Region::open(path);
	if (!region) {
		std::cerr << "open failed: " << region.error().message() << '\n';
		return 1;
	}
	std::printf("%-6s %-6s %-10s %-12s\n", "cx", "cz", "timestamp", "size");
	for (int cz = 0; cz < tagforge::Region::kSize; ++cz) {
		for (int cx = 0; cx < tagforge::Region::kSize; ++cx) {
			if (!region->has_chunk(cx, cz)) {
				continue;
			}
			auto raw = region->chunk_raw(cx, cz);
			if (!raw) {
				std::printf("%-6d %-6d %-10u <error: %.*s>\n", cx, cz, region->chunk_timestamp(cx, cz),
					    static_cast<int>(raw.error().message().size()),
					    raw.error().message().data());
				continue;
			}
			std::printf("%-6d %-6d %-10u %-12zu\n", cx, cz, region->chunk_timestamp(cx, cz), raw->size());
		}
	}
	return 0;
}

int verify_one(const std::filesystem::path &p, const std::string &fmt_name)
{
	try {
		auto bytes = decompress_if_compressed(slurp(p));
		const auto fmt = parse_format(fmt_name);
		auto v = tagforge::decode(bytes, fmt);
		if (!v) {
			std::cerr << p << ": decode error at offset " << v.error().offset << ": " << v.error().message()
				  << '\n';
			return 1;
		}
		auto enc = tagforge::encode(*v, fmt);
		if (!enc) {
			std::cerr << p << ": encode error: " << enc.error().message() << '\n';
			return 1;
		}
		if (*enc != bytes) {
			std::cerr << p << ": round-trip byte mismatch (" << enc->size() << " vs " << bytes.size()
				  << ")\n";
			return 1;
		}
		std::cout << p << ": OK (" << bytes.size() << " bytes)\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << p << ": exception: " << e.what() << '\n';
		return 1;
	}
}

int cmd_verify(const std::vector<std::string> &paths, const std::string &fmt_name)
{
	int failures = 0;
	for (const auto &raw_path : paths) {
		std::filesystem::path p{raw_path};
		if (std::filesystem::is_directory(p)) {
			for (const auto &entry : std::filesystem::recursive_directory_iterator(p)) {
				if (!entry.is_regular_file()) {
					continue;
				}
				const auto ext = entry.path().extension().string();
				if (ext != ".nbt") {
					continue;
				}
				failures += verify_one(entry.path(), fmt_name);
			}
		} else {
			failures += verify_one(p, fmt_name);
		}
	}
	return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
	CLI::App app{"tagforge nbt-cli"};
	app.require_subcommand(1);

	std::string fmt_name = "auto";
	bool pretty = false;
	int indent = 2;
	std::string path;
	std::vector<std::string> paths;

	auto *dump = app.add_subcommand("dump", "Decode and pretty-print an NBT file (SNBT to stdout)");
	dump->add_option("file", path, "Input file")->required();
	dump->add_option("--format", fmt_name, "Wire format (auto | java | java-net | bedrock-le | bedrock-varint)")
		->default_str("auto");
	dump->add_flag("--pretty", pretty, "Multi-line output");
	dump->add_option("--indent", indent, "Indent width (with --pretty)")->default_val(2);
	dump->callback([&] { std::exit(cmd_dump(path, fmt_name, pretty, indent)); });

	auto *tosnbt = app.add_subcommand("to-snbt", "Convert binary NBT to SNBT");
	tosnbt->add_option("file", path, "Input file")->required();
	tosnbt->add_option("--format", fmt_name, "Wire format")->default_str("auto");
	tosnbt->add_flag("--pretty", pretty);
	tosnbt->callback([&] { std::exit(cmd_to_snbt(path, fmt_name, pretty)); });

	auto *fromsnbt = app.add_subcommand("from-snbt", "Convert SNBT to binary NBT (stdout)");
	fromsnbt->add_option("file", path, "SNBT input")->required();
	fromsnbt->add_option("--format", fmt_name, "Output wire format")->default_str("java-net");
	fromsnbt->callback([&] { std::exit(cmd_from_snbt(path, fmt_name)); });

	auto *verify = app.add_subcommand("verify", "Decode + encode each file and assert byte equality");
	verify->add_option("paths", paths, "Files or directories")->required();
	verify->add_option("--format", fmt_name, "Wire format")->default_str("java");
	verify->callback([&] { std::exit(cmd_verify(paths, fmt_name)); });

	auto *dec = app.add_subcommand("decompress", "Auto-detect gzip/zlib; write raw bytes to stdout");
	dec->add_option("file", path)->required();
	dec->callback([&] { std::exit(cmd_decompress(path)); });

	auto *rgn = app.add_subcommand("region-list", "List populated chunks in a region (.mca) file");
	rgn->add_option("file", path)->required();
	rgn->callback([&] { std::exit(cmd_region_list(path)); });

	CLI11_PARSE(app, argc, argv);
	return 0;
}
