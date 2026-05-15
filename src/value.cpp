// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Matthew Beshay

#include "tagforge/value.hpp"

#include <algorithm>
#include <span>

namespace tagforge {

namespace {

constexpr std::string_view kMissingDetail = "key missing";
constexpr std::string_view kWrongArmDetail = "wrong tag arm";

template<class ArmT> [[nodiscard]] std::expected<ArmT, Error> typed_get_value(const Compound &c, std::string_view name)
{
	const Value *v = find(c, name);
	if (!v) {
		return std::unexpected(make_error(ErrorCode::UnknownTagId, 0, kMissingDetail));
	}
	if (const ArmT *arm = std::get_if<ArmT>(&v->v)) {
		return *arm;
	}
	return std::unexpected(make_error(ErrorCode::UnexpectedRootType, 0, kWrongArmDetail));
}

template<class ArmT>
[[nodiscard]] std::expected<const ArmT *, Error> typed_get_pointer(const Compound &c, std::string_view name)
{
	const Value *v = find(c, name);
	if (!v) {
		return std::unexpected(make_error(ErrorCode::UnknownTagId, 0, kMissingDetail));
	}
	if (const ArmT *arm = std::get_if<ArmT>(&v->v)) {
		return arm;
	}
	return std::unexpected(make_error(ErrorCode::UnexpectedRootType, 0, kWrongArmDetail));
}

} // namespace

TagId Value::list_arm() const noexcept
{
	if (kind() != TagId::List) {
		return TagId::End;
	}
	const auto &list = std::get<List>(v);
	if (list.empty()) {
		return TagId::End;
	}
	return list.front().kind();
}

const Value *find(const Compound &c, std::string_view name) noexcept
{
	for (const auto &entry : c) {
		if (entry.first == name) {
			return &entry.second;
		}
	}
	return nullptr;
}

Value *find(Compound &c, std::string_view name) noexcept
{
	for (auto &entry : c) {
		if (entry.first == name) {
			return &entry.second;
		}
	}
	return nullptr;
}

Value &upsert(Compound &c, std::string name, Value value)
{
	for (auto &entry : c) {
		if (entry.first == name) {
			entry.second = std::move(value);
			return entry.second;
		}
	}
	c.emplace_back(std::move(name), std::move(value));
	return c.back().second;
}

std::expected<std::int8_t, Error> get_byte(const Compound &c, std::string_view n)
{
	return typed_get_value<std::int8_t>(c, n);
}
std::expected<std::int16_t, Error> get_short(const Compound &c, std::string_view n)
{
	return typed_get_value<std::int16_t>(c, n);
}
std::expected<std::int32_t, Error> get_int(const Compound &c, std::string_view n)
{
	return typed_get_value<std::int32_t>(c, n);
}
std::expected<std::int64_t, Error> get_long(const Compound &c, std::string_view n)
{
	return typed_get_value<std::int64_t>(c, n);
}
std::expected<float, Error> get_float(const Compound &c, std::string_view n)
{
	return typed_get_value<float>(c, n);
}
std::expected<double, Error> get_double(const Compound &c, std::string_view n)
{
	return typed_get_value<double>(c, n);
}

std::expected<bool, Error> get_bool(const Compound &c, std::string_view n)
{
	auto b = get_byte(c, n);
	if (!b) {
		return std::unexpected(b.error());
	}
	return *b != 0;
}

std::expected<std::string_view, Error> get_string(const Compound &c, std::string_view n)
{
	auto p = typed_get_pointer<std::string>(c, n);
	if (!p) {
		return std::unexpected(p.error());
	}
	return std::string_view{**p};
}

std::expected<std::span<const std::int8_t>, Error> get_byte_array(const Compound &c, std::string_view n)
{
	auto p = typed_get_pointer<std::vector<std::int8_t>>(c, n);
	if (!p) {
		return std::unexpected(p.error());
	}
	return std::span<const std::int8_t>{**p};
}

std::expected<std::span<const std::int32_t>, Error> get_int_array(const Compound &c, std::string_view n)
{
	auto p = typed_get_pointer<std::vector<std::int32_t>>(c, n);
	if (!p) {
		return std::unexpected(p.error());
	}
	return std::span<const std::int32_t>{**p};
}

std::expected<std::span<const std::int64_t>, Error> get_long_array(const Compound &c, std::string_view n)
{
	auto p = typed_get_pointer<std::vector<std::int64_t>>(c, n);
	if (!p) {
		return std::unexpected(p.error());
	}
	return std::span<const std::int64_t>{**p};
}

std::expected<const Compound *, Error> get_compound(const Compound &c, std::string_view n)
{
	return typed_get_pointer<Compound>(c, n);
}

std::expected<const List *, Error> get_list(const Compound &c, std::string_view n)
{
	return typed_get_pointer<List>(c, n);
}

} // namespace tagforge
