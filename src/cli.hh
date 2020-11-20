#pragma once

#include "parse_traits.hh"
#include <concepts>
#include <type_traits>
#include <variant>

template <typename Base, typename Predicate>
struct WithCheck : public Base
{
    constexpr explicit WithCheck(Base base, Predicate predicate_, std::string_view error_message_) : Base(base), validation_predicate(predicate_), error_message(error_message_) {}

    constexpr std::optional<typename Base::parse_result_type> parse(int argc, char const * const argv[]) const noexcept
    {
        auto result = Base::parse(argc, argv);
        if (!result || !validate(*result))
            return std::nullopt;
        else
            return result;
    }

    template <typename OtherPredicate>
    constexpr WithCheck<WithCheck<Base, Predicate>, OtherPredicate> check(OtherPredicate other_predicate, std::string_view other_error_message) const noexcept
    {
        return WithCheck<WithCheck<Base, Predicate>, OtherPredicate>(*this, other_predicate, other_error_message);
    }

private:
    Predicate validation_predicate;
    std::string_view error_message;

    constexpr bool validate(typename Base::parse_result_type t) const noexcept
    {
        return validation_predicate(t._get());
    }
};

template <typename Base>
struct WithDefaultValue : public Base
{
    constexpr explicit WithDefaultValue(Base base, typename Base::value_type default_value_) noexcept : Base(base), default_value(std::move(default_value_)) {}

    constexpr std::optional<typename Base::parse_result_type> parse(int argc, char const * const argv[]) const noexcept
    {
        auto result = Base::parse(argc, argv);
        if (!result)
            return typename Base::parse_result_type{ default_value };
        else
            return result;
    }

    template <typename Predicate>
    constexpr WithCheck<WithDefaultValue<Base>, Predicate> check(Predicate predicate, std::string_view error_message) const noexcept
    {
        return WithCheck<WithDefaultValue<Base>, Predicate>(*this, predicate, error_message);
    }

private:
    typename Base::value_type default_value;
};

template <typename Base>
struct WithDescription : public Base
{
    std::string_view description;

    constexpr void operator () (std::string_view) const noexcept = delete;

    constexpr WithDefaultValue<WithDescription<Base>> default_to(typename Base::value_type default_value) const noexcept
    {
        return WithDefaultValue<WithDescription<Base>>(*this, std::move(default_value));
    }

    template <typename Predicate>
    constexpr WithCheck<WithDescription<Base>, Predicate> check(Predicate predicate, std::string_view error_message) const noexcept
    {
        return WithCheck<WithDescription<Base>, Predicate>(*this, predicate, error_message);
    }
};

template <typename T>
concept Pattern = requires (T pattern, std::string_view text) { { pattern.match(text) } -> std::same_as<std::optional<std::string_view>>; };

template <typename Base>
struct WithPattern : public Base
{
    constexpr explicit WithPattern(Base base, std::string_view pattern_) : Base(base), pattern(pattern_) {}

    constexpr std::optional<std::string_view> match(std::string_view text) const noexcept
    {
        if constexpr (Pattern<Base>)
        {
            std::optional<std::string_view> const matched = Base::match(text);
            if (matched)
                return matched;
        }

        if (text.starts_with(pattern) && text[pattern.size()] == '=')
            return text.substr(pattern.size() + 1);
        else
            return std::nullopt;
    }

    constexpr std::optional<typename Base::parse_result_type> parse(int argc, char const * const argv[]) const noexcept
    {
        for (int i = 0; i < argc; ++i)
        {
            auto const matched = match(argv[i]);
            if (matched)
                return Base::parse_impl(*matched);
        }
        return std::nullopt;
    }

    constexpr WithDescription<WithPattern<Base>> operator () (std::string_view description) const noexcept { return WithDescription<WithPattern<Base>>{*this, description}; }
    constexpr WithPattern<WithPattern<Base>> operator [] (std::string_view extra_pattern) const noexcept { return WithPattern<WithPattern<Base>>(*this, extra_pattern); };

private:
    std::string_view pattern;
};

template <typename T>
concept OptionStruct = requires(T a) { { a._get() } -> std::same_as<typename T::value_type const &>; };

template <OptionStruct T>
struct Option
{
    using parse_result_type = T;
    using value_type = typename T::value_type;

    constexpr explicit Option(std::string_view type_name_) : type_name(type_name_) {}
    std::string_view type_name;
    std::optional<T> parse_impl(std::string_view argument_text) const noexcept
    {
        std::optional<value_type> value = parse_traits<value_type>::parse(argument_text);
        if (value)
            return T{ std::move(*value) };
        else
            return std::nullopt;
    }

    constexpr WithDescription<Option<T>> operator () (std::string_view description) const noexcept { return WithDescription<Option<T>>{*this, description}; }
    constexpr WithPattern<Option<T>> operator [] (std::string_view pattern) const noexcept { return WithPattern<Option<T>>(*this, pattern); }
};

#define Opt(type, var) Option<std::remove_pointer_t<decltype([](){                                      \
    struct OptionTypeImpl                                                                               \
    {                                                                                                   \
        using value_type = type;                                                                        \
        type var;                                                                                       \
        constexpr type const & _get() const noexcept { return var; }                                    \
    };                                                                                                  \
    return static_cast<OptionTypeImpl *>(nullptr);                                                      \
}())>>(#type)

template <typename T>
using get_parse_result_type = typename T::parse_result_type;

template <typename T>
concept Parser = requires(T const parser, int argc, char const * const argv[]) { { parser.parse(argc, argv) } -> std::same_as<std::optional<typename T::parse_result_type>>; };

template <Parser ... Options>
struct Compound : private Options...
{
    constexpr explicit Compound(Options... options) : Options(options)... {}

    struct parse_result_type : public get_parse_result_type<Options>... {};

    constexpr std::optional<parse_result_type> parse(int argc, char const * const argv[]) const noexcept
    {
        return[...opts = static_cast<Options const *>(this)->parse(argc, argv)]() noexcept -> std::optional<parse_result_type>
        {
            if (!(opts && ...))
                return std::nullopt;
            else
                return parse_result_type{ std::move(*opts)... };
        }();
    }

    template <Parser T>
    constexpr T const & access_option() const noexcept
    {
        return static_cast<T const &>(*this);
    }
};

template <Parser A, Parser B>
constexpr Compound<A, B> operator | (A a, B b) noexcept
{
    return Compound<A, B>(a, b);
}

template <Parser ... A, Parser B>
constexpr Compound<A..., B> operator | (Compound<A...> a, B b) noexcept
{
    return Compound<A..., B>(a.template access_option<A>()..., b);
}

template <Parser A, Parser ... B>
constexpr Compound<A, B...> operator | (A a, Compound<B...> b) noexcept
{
    return Compound<A, B...>(a, b.template access_option<B>()...);
}

template <Parser ... A, Parser ... B>
constexpr Compound<A..., B...> operator | (Compound<A...> a, Compound<B...> b) noexcept
{
    return Compound<A..., B...>(
        a.template access_option<A>()...,
        b.template access_option<B>()...);
}

template <Parser P>
struct Command
{
    explicit constexpr Command(std::string_view name_, P parser_) noexcept : name(name_), parser(parser_) {}

    std::string_view name;
    P parser;
};

template <Parser ... Subparsers>
struct Commands : private Command<Subparsers>...
{
    using parse_result_type = std::variant<get_parse_result_type<Subparsers>...>;

    constexpr explicit Commands(Command<Subparsers>... commands) noexcept : Command<Subparsers>(commands)... {}

    constexpr std::optional<parse_result_type> parse(int argc, char const * const argv[]) const noexcept
    {
        return parse_impl<0, Subparsers...>(argc, argv);
    }

    template <Parser T>
    constexpr T const & access_subparser() const noexcept
    {
        return static_cast<T const &>(*this);
    }

private:
    template <int>
    constexpr std::optional<parse_result_type> parse_impl(int, char const * const[]) const noexcept
    {
        return std::nullopt;
    }
    
    template <int, typename Next, typename ... Rest>
    constexpr std::optional<parse_result_type> parse_impl(int argc, char const * const argv[]) const noexcept
    {
        Command<Next> const & next = *this;
        if (argc > 0 && next.name == argv[0])
        {
            auto result = next.parser.parse(argc - 1, argv + 1);
            if (!result)
                return std::nullopt;
            else
                return std::move(*result);
        }
        else
        {
            return parse_impl<0, Rest...>(argc, argv);
        }
    }
};

template <Parser A, Parser B>
constexpr Commands<A, B> operator & (Command<A> a, Command<B> b) noexcept
{
    return Commands<A, B>(a, b);
}

template <Parser ... A, Parser B>
constexpr Commands<A..., B> operator & (Commands<A...> a, Command<B> b) noexcept
{
    return Commands<A..., B>(a.template access_subparser<A>()..., b);
}

template <Parser A, Parser ... B>
constexpr Commands<A, B...> operator & (Command<A> a, Commands<B...> b) noexcept
{
    return Commands<A, B...>(a, b.template access_subparser<B>()...);
}

template <Parser ... A, Parser ... B>
constexpr Commands<A..., B...> operator & (Commands<A...> a, Commands<B...> b) noexcept
{
    return Commands<A..., B...>(a.template access_subparser<A>()..., b.template access_subparser<B>()...);
}
