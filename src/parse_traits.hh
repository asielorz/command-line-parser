#pragma once

#include <string_view>
#include <charconv>
#include <string>
#include <optional>

template <typename T>
struct parse_traits;

template <typename T>
std::optional<T> charconv_parse(std::string_view text) noexcept
{
	T x;
	auto const result = std::from_chars(text.data(), text.data() + text.size(), x);
	if (result.ec != std::errc() || result.ptr != text.data() + text.size())
		return std::nullopt;
	return x;
}

template <>
struct parse_traits<int>
{
	static std::optional<int> parse(std::string_view text) noexcept
	{
		return charconv_parse<int>(text);
	}
};

template <>
struct parse_traits<float>
{
	static std::optional<float> parse(std::string_view text) noexcept
	{
		return charconv_parse<float>(text);
	}
};

template <>
struct parse_traits<bool>
{
	static constexpr std::optional<bool> parse(std::string_view text) noexcept
	{
		if (text == "true")
			return true;
		else if (text == "false")
			return false;
		else
			return std::nullopt;
	}
};

template <>
struct parse_traits<std::string>
{
	static std::optional<std::string> parse(std::string_view text) noexcept
	{
		return std::string(text);
	}
};
