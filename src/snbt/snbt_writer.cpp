// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "tagforge/snbt.hpp"

#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace tagforge {

namespace {

bool needs_quoting(std::string_view s)
{
	if (s.empty()) {
		return true;
	}
	for (char c : s) {
		const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				c == '_' || c == '-' || c == '.' || c == '+';
		if (!ok) {
			return true;
		}
	}
	return false;
}

void emit_quoted(std::string &out, std::string_view s)
{
	out.push_back('"');
	for (char c : s) {
		if (c == '"' || c == '\\') {
			out.push_back('\\');
		}
		out.push_back(c);
	}
	out.push_back('"');
}

void emit_key(std::string &out, std::string_view key, const SnbtOptions &opts)
{
	if (opts.quote_all_keys || needs_quoting(key)) {
		emit_quoted(out, key);
	} else {
		out.append(key);
	}
}

void emit_number_int(std::string &out, long long v)
{
	char buf[32];
	auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), v);
	out.append(buf, p);
}

void emit_double(std::string &out, double v)
{
	char buf[32];
	auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), v);
	out.append(buf, p);
}

void emit_float(std::string &out, float v)
{
	char buf[32];
	auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), v);
	out.append(buf, p);
}

void emit_indent(std::string &out, std::size_t depth, std::uint8_t width)
{
	out.append(static_cast<std::size_t>(depth) * width, ' ');
}

void emit_value(std::string &out, const Value &v, const SnbtOptions &opts, std::size_t depth);

void emit_value(std::string &out, const Value &v, const SnbtOptions &opts, std::size_t depth)
{
	switch (v.kind()) {
	case TagId::End:
		out.append("END");
		return;
	case TagId::Byte:
		emit_number_int(out, std::get<std::int8_t>(v.v));
		out.push_back('b');
		return;
	case TagId::Short:
		emit_number_int(out, std::get<std::int16_t>(v.v));
		out.push_back('s');
		return;
	case TagId::Int:
		emit_number_int(out, std::get<std::int32_t>(v.v));
		return;
	case TagId::Long:
		emit_number_int(out, std::get<std::int64_t>(v.v));
		out.push_back('L');
		return;
	case TagId::Float:
		emit_float(out, std::get<float>(v.v));
		out.push_back('f');
		return;
	case TagId::Double:
		emit_double(out, std::get<double>(v.v));
		out.push_back('d');
		return;
	case TagId::String:
		emit_quoted(out, std::get<std::string>(v.v));
		return;
	case TagId::ByteArray: {
		out.append(opts.array_suffix_caps ? "[B;" : "[b;");
		const auto &arr = std::get<std::vector<std::int8_t>>(v.v);
		for (std::size_t i = 0; i < arr.size(); ++i) {
			if (i) {
				out.push_back(',');
			}
			emit_number_int(out, arr[i]);
			out.push_back('b');
		}
		out.push_back(']');
		return;
	}
	case TagId::IntArray: {
		out.append(opts.array_suffix_caps ? "[I;" : "[i;");
		const auto &arr = std::get<std::vector<std::int32_t>>(v.v);
		for (std::size_t i = 0; i < arr.size(); ++i) {
			if (i) {
				out.push_back(',');
			}
			emit_number_int(out, arr[i]);
		}
		out.push_back(']');
		return;
	}
	case TagId::LongArray: {
		out.append(opts.array_suffix_caps ? "[L;" : "[l;");
		const auto &arr = std::get<std::vector<std::int64_t>>(v.v);
		for (std::size_t i = 0; i < arr.size(); ++i) {
			if (i) {
				out.push_back(',');
			}
			emit_number_int(out, arr[i]);
			out.push_back('L');
		}
		out.push_back(']');
		return;
	}
	case TagId::List: {
		const auto &list = std::get<List>(v.v);
		out.push_back('[');
		if (opts.pretty && !list.empty()) {
			out.push_back('\n');
		}
		for (std::size_t i = 0; i < list.size(); ++i) {
			if (opts.pretty) {
				emit_indent(out, depth + 1, opts.indent_width);
			}
			emit_value(out, list[i], opts, depth + 1);
			if (i + 1 < list.size()) {
				out.push_back(',');
			}
			if (opts.pretty) {
				out.push_back('\n');
			}
		}
		if (opts.pretty && !list.empty()) {
			emit_indent(out, depth, opts.indent_width);
		}
		out.push_back(']');
		return;
	}
	case TagId::Compound: {
		const auto &c = std::get<Compound>(v.v);
		out.push_back('{');
		if (opts.pretty && !c.empty()) {
			out.push_back('\n');
		}
		for (std::size_t i = 0; i < c.size(); ++i) {
			if (opts.pretty) {
				emit_indent(out, depth + 1, opts.indent_width);
			}
			emit_key(out, c[i].first, opts);
			out.push_back(':');
			if (opts.pretty) {
				out.push_back(' ');
			}
			emit_value(out, c[i].second, opts, depth + 1);
			if (i + 1 < c.size()) {
				out.push_back(',');
			}
			if (opts.pretty) {
				out.push_back('\n');
			}
		}
		if (opts.pretty && !c.empty()) {
			emit_indent(out, depth, opts.indent_width);
		}
		out.push_back('}');
		return;
	}
	}
}

} // namespace

std::string to_snbt(const Value &v, const SnbtOptions &opts)
{
	std::string out;
	emit_value(out, v, opts, 0);
	return out;
}

std::string to_snbt(const NamedValue &nv, const SnbtOptions &opts)
{
	return to_snbt(nv.value, opts);
}

} // namespace tagforge
