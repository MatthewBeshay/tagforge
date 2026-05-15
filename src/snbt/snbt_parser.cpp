// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay
//
// Mojangson (SNBT) parser. Handles the syntax accepted by Mojang's `/data`
// command, including type-suffixed numbers, typed arrays (`[B;...]`,
// `[I;...]`, `[L;...]`), boolean keywords, and quoted strings.

#include "tagforge/snbt.hpp"

#include "tagforge/error.hpp"
#include "tagforge/tag_id.hpp"
#include "tagforge/value.hpp"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tagforge {

namespace {

[[nodiscard]] constexpr bool is_unquoted_id_char(char c) noexcept
{
	// Unquoted SNBT identifier characters. Mojangson allows '.' here
	// (unlike NBT-path syntax, where '.' is the segment separator).
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
	       c == '.' || c == '+';
}

class Parser {
public:
	explicit Parser(std::string_view text) : text_{text} {}

	std::expected<Value, Error> parse_root()
	{
		skip_ws();
		auto v = parse_value();
		if (!v) {
			return v;
		}
		skip_ws();
		if (pos_ != text_.size()) {
			return std::unexpected(syntax("trailing content after value"));
		}
		return v;
	}

private:
	std::string_view text_;
	std::size_t pos_ = 0;

	Error syntax(std::string_view detail) const { return make_error(ErrorCode::SnbtSyntax, pos_, detail); }

	Error range_err() const { return make_error(ErrorCode::SnbtNumberOutOfRange, pos_); }

	void skip_ws()
	{
		while (pos_ < text_.size()) {
			const char c = text_[pos_];
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
				++pos_;
			} else {
				break;
			}
		}
	}

	bool peek(char c) const noexcept { return pos_ < text_.size() && text_[pos_] == c; }

	bool consume(char c)
	{
		if (peek(c)) {
			++pos_;
			return true;
		}
		return false;
	}

	std::expected<Value, Error> parse_value()
	{
		skip_ws();
		if (pos_ >= text_.size()) {
			return std::unexpected(syntax("unexpected end of input"));
		}
		const char c = text_[pos_];
		switch (c) {
		case '{':
			return parse_compound();
		case '[':
			return parse_array_or_list();
		case '"':
		case '\'':
			return parse_quoted_string();
		default:
			return parse_unquoted_or_number();
		}
	}

	std::expected<Value, Error> parse_compound()
	{
		++pos_; // consume {
		Compound c;
		skip_ws();
		if (consume('}')) {
			return Value{.v = std::move(c)};
		}
		while (true) {
			skip_ws();
			auto key = parse_key();
			if (!key) {
				return std::unexpected(key.error());
			}
			skip_ws();
			if (!consume(':')) {
				return std::unexpected(syntax("expected ':' after key"));
			}
			auto val = parse_value();
			if (!val) {
				return val;
			}
			c.emplace_back(std::move(*key), std::move(*val));
			skip_ws();
			if (consume(',')) {
				continue;
			}
			if (consume('}')) {
				return Value{.v = std::move(c)};
			}
			return std::unexpected(syntax("expected ',' or '}' inside compound"));
		}
	}

	std::expected<std::string, Error> parse_key()
	{
		if (peek('"') || peek('\'')) {
			auto s = parse_quoted_string();
			if (!s) {
				return std::unexpected(s.error());
			}
			return std::move(std::get<std::string>(s->v));
		}
		return parse_unquoted_identifier();
	}

	std::expected<std::string, Error> parse_unquoted_identifier()
	{
		const std::size_t start = pos_;
		while (pos_ < text_.size() && is_unquoted_id_char(text_[pos_])) {
			++pos_;
		}
		if (pos_ == start) {
			return std::unexpected(syntax("expected key"));
		}
		return std::string{text_.substr(start, pos_ - start)};
	}

	std::expected<Value, Error> parse_quoted_string()
	{
		const char quote = text_[pos_++];
		std::string out;
		while (pos_ < text_.size()) {
			const char c = text_[pos_];
			if (c == quote) {
				++pos_;
				return Value{.v = std::move(out)};
			}
			if (c == '\\') {
				if (pos_ + 1 >= text_.size()) {
					return std::unexpected(syntax("unterminated escape"));
				}
				const char esc = text_[pos_ + 1];
				if (esc == '"' || esc == '\'' || esc == '\\') {
					out.push_back(esc);
				} else {
					return std::unexpected(syntax("unrecognised escape"));
				}
				pos_ += 2;
				continue;
			}
			out.push_back(c);
			++pos_;
		}
		return std::unexpected(syntax("unterminated string"));
	}

	std::expected<Value, Error> parse_array_or_list()
	{
		++pos_; // consume [
		// Typed-array prefix: [B;..], [I;..], [L;..]
		if (pos_ + 1 < text_.size() && text_[pos_ + 1] == ';') {
			const char tag = text_[pos_];
			if (tag == 'B' || tag == 'I' || tag == 'L') {
				pos_ += 2; // consume "B;" / "I;" / "L;"
				skip_ws();
				if (consume(']')) {
					return empty_typed_array(tag);
				}
				if (tag == 'B') {
					std::vector<std::int8_t> data;
					if (auto e = parse_typed_array_elements(data); !e) {
						return std::unexpected(e.error());
					}
					return Value{.v = std::move(data)};
				} else if (tag == 'I') {
					std::vector<std::int32_t> data;
					if (auto e = parse_typed_array_elements(data); !e) {
						return std::unexpected(e.error());
					}
					return Value{.v = std::move(data)};
				} else {
					std::vector<std::int64_t> data;
					if (auto e = parse_typed_array_elements(data); !e) {
						return std::unexpected(e.error());
					}
					return Value{.v = std::move(data)};
				}
			}
		}
		// Generic list.
		skip_ws();
		if (consume(']')) {
			return Value{.v = List{}};
		}
		List list;
		while (true) {
			auto v = parse_value();
			if (!v) {
				return v;
			}
			if (!list.empty() && v->kind() != list.front().kind()) {
				return std::unexpected(make_error(ErrorCode::MixedListArm, pos_));
			}
			list.push_back(std::move(*v));
			skip_ws();
			if (consume(',')) {
				continue;
			}
			if (consume(']')) {
				return Value{.v = std::move(list)};
			}
			return std::unexpected(syntax("expected ',' or ']' inside list"));
		}
	}

	static Value empty_typed_array(char tag)
	{
		switch (tag) {
		case 'B':
			return Value{.v = std::vector<std::int8_t>{}};
		case 'I':
			return Value{.v = std::vector<std::int32_t>{}};
		case 'L':
			return Value{.v = std::vector<std::int64_t>{}};
		}
		return Value{};
	}

	template<class T> std::expected<void, Error> parse_typed_array_elements(std::vector<T> &out)
	{
		while (true) {
			skip_ws();
			auto v = parse_number();
			if (!v) {
				return std::unexpected(v.error());
			}
			T cast = static_cast<T>(*v);
			if (static_cast<long long>(cast) != *v) {
				return std::unexpected(range_err());
			}
			out.push_back(cast);
			skip_ws();
			if (consume(',')) {
				continue;
			}
			if (consume(']')) {
				return {};
			}
			return std::unexpected(syntax("expected ',' or ']' inside typed array"));
		}
	}

	std::expected<long long, Error> parse_number()
	{
		const std::size_t start = pos_;
		if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
			++pos_;
		}
		bool has_digit = false;
		while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
			++pos_;
			has_digit = true;
		}
		if (!has_digit) {
			return std::unexpected(syntax("expected number"));
		}
		// Optional suffix b/B/s/S/L (no decimal).
		if (pos_ < text_.size()) {
			const char c = text_[pos_];
			if (c == 'b' || c == 'B' || c == 's' || c == 'S' || c == 'L') {
				++pos_;
			}
		}
		long long out = 0;
		auto sv = text_.substr(start, pos_ - start);
		// Strip suffix for conversion.
		while (!sv.empty() && std::isalpha(static_cast<unsigned char>(sv.back()))) {
			sv.remove_suffix(1);
		}
		auto [p, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
		if (ec != std::errc{}) {
			return std::unexpected(range_err());
		}
		return out;
	}

	std::expected<Value, Error> parse_unquoted_or_number()
	{
		const std::size_t start = pos_;
		while (pos_ < text_.size() && is_unquoted_id_char(text_[pos_])) {
			++pos_;
		}
		if (pos_ == start) {
			return std::unexpected(syntax("expected value"));
		}
		std::string_view lex = text_.substr(start, pos_ - start);
		// Keywords.
		if (lex == "true") {
			return Value{.v = std::int8_t{1}};
		}
		if (lex == "false") {
			return Value{.v = std::int8_t{0}};
		}

		// Determine numeric form.
		char suffix = '\0';
		std::string_view body = lex;
		if (!body.empty()) {
			const char last = body.back();
			if (last == 'b' || last == 'B' || last == 's' || last == 'S' || last == 'L' || last == 'f' ||
			    last == 'F' || last == 'd' || last == 'D') {
				suffix = last;
				body.remove_suffix(1);
			}
		}
		const bool has_dot = body.find('.') != std::string_view::npos ||
				     body.find('e') != std::string_view::npos ||
				     body.find('E') != std::string_view::npos;

		if (body.empty()) {
			// It was just letters → treat as a bare-word string.
			return Value{.v = std::string{lex}};
		}
		bool ok_number = true;
		std::size_t i = 0;
		if (body[i] == '+' || body[i] == '-') {
			++i;
		}
		if (i >= body.size()) {
			ok_number = false;
		}
		for (; i < body.size(); ++i) {
			const char c = body[i];
			if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == 'e' || c == 'E' ||
			      c == '+' || c == '-')) {
				ok_number = false;
				break;
			}
		}

		if (!ok_number) {
			return Value{.v = std::string{lex}};
		}

		if (has_dot || suffix == 'f' || suffix == 'F' || suffix == 'd' || suffix == 'D') {
			double d = 0.0;
			auto [p, ec] = std::from_chars(body.data(), body.data() + body.size(), d);
			if (ec != std::errc{}) {
				return std::unexpected(range_err());
			}
			if (suffix == 'f' || suffix == 'F') {
				return Value{.v = static_cast<float>(d)};
			}
			return Value{.v = d};
		}

		long long ll = 0;
		auto [p, ec] = std::from_chars(body.data(), body.data() + body.size(), ll);
		if (ec != std::errc{}) {
			return std::unexpected(range_err());
		}
		switch (suffix) {
		case 'b':
		case 'B':
			return Value{.v = static_cast<std::int8_t>(ll)};
		case 's':
		case 'S':
			return Value{.v = static_cast<std::int16_t>(ll)};
		case 'L':
			return Value{.v = static_cast<std::int64_t>(ll)};
		}
		if (ll < std::numeric_limits<std::int32_t>::min() || ll > std::numeric_limits<std::int32_t>::max()) {
			return Value{.v = static_cast<std::int64_t>(ll)};
		}
		return Value{.v = static_cast<std::int32_t>(ll)};
	}
};

} // namespace

std::expected<Value, Error> parse_snbt(std::string_view text)
{
	Parser p{text};
	return p.parse_root();
}

} // namespace tagforge
