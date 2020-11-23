#pragma once

#include "parse_traits.hh"
#include <concepts>
#include <type_traits>
#include <variant>

namespace clp
{

    template <typename T>
    concept HasValidationCheck = requires(T option, typename T::parse_result_type parse_result) {
        {option.validate(parse_result)} -> std::same_as<bool>;
    };

    template <typename Base, typename Predicate>
    struct WithCheck : public Base
    {
        constexpr explicit WithCheck(Base base, Predicate predicate_, std::string_view error_message_) : Base(base), validation_predicate(predicate_), error_message(error_message_) {}

        constexpr bool validate(typename Base::parse_result_type t) const noexcept
        {
            if constexpr (HasValidationCheck<Base>)
                if (!Base::validate(t))
                    return false;

            return validation_predicate(t._get());
        }

    private:
        Predicate validation_predicate;
        std::string_view error_message;
    };

    template <typename Base>
    struct WithImplicitValue : public Base
    {
        constexpr explicit WithImplicitValue(Base base, typename Base::value_type implicit_value_) noexcept : Base(base), implicit_value(std::move(implicit_value_)) {}

        typename Base::value_type implicit_value;
    };

    template <typename T>
    concept HasImplicitValue = requires(T option) {
        {option.implicit_value} -> std::convertible_to<typename T::value_type>;
    };

    template <typename Base>
    struct WithDefaultValue : public Base
    {
        constexpr explicit WithDefaultValue(Base base, typename Base::value_type default_value_) noexcept : Base(base), default_value(std::move(default_value_)) {}

        typename Base::value_type default_value;
    };

    template <typename T>
    concept HasDefaultValue = requires(T option) {
        {option.default_value} -> std::convertible_to<typename T::value_type>;
    };

    template <typename Base>
    struct WithDescription : public Base
    {
        std::string_view description;
    };

    template <typename T>
    concept HasDescription = requires(T option) {
        {option.description} -> std::same_as<std::string_view>;
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

            if (text.starts_with(pattern))
            {
                if (text.size() == pattern.size())
                    return "";
                else if (text[pattern.size()] == '=')
                    return text.substr(pattern.size() + 1);
            }

            return std::nullopt;
        }

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
                return T{std::move(*value)};
            else
                return std::nullopt;
        }
    };

    template <typename Base>
    struct OptionInterface : public Base
    {
        explicit constexpr OptionInterface(Base base) noexcept : Base(base) {}

        constexpr OptionInterface<WithDescription<Base>> operator () (std::string_view description) const noexcept requires(!HasDescription<Base>)
        {
            return OptionInterface<WithDescription<Base>>(WithDescription<Base>{*this, description});
        }

        constexpr OptionInterface<WithPattern<Base>> operator [] (std::string_view pattern) const noexcept 
        {
            return OptionInterface<WithPattern<Base>>(WithPattern<Base>(*this, pattern));
        }

        constexpr OptionInterface<WithDefaultValue<Base>> default_to(typename Base::value_type default_value) const noexcept requires(!HasDefaultValue<Base>)
        {
            return OptionInterface<WithDefaultValue<Base>>(WithDefaultValue<Base>(*this, std::move(default_value)));
        }

        constexpr OptionInterface<WithImplicitValue<Base>> implicitly(typename Base::value_type implicit_value) const noexcept requires(!HasImplicitValue<Base>)
        {
            return OptionInterface<WithImplicitValue<Base>>(WithImplicitValue<Base>(*this, std::move(implicit_value)));
        }

        template <typename Predicate>
        constexpr OptionInterface<WithCheck<Base, Predicate>> check(Predicate predicate, std::string_view error_message) const noexcept
        {
            return OptionInterface<WithCheck<Base, Predicate>>(WithCheck<Base, Predicate>(*this, predicate, error_message));
        }
    };

    template <typename T>
    concept SingleOption = requires(T option, std::string_view arg) {
        {option.match(arg)} -> std::same_as<std::optional<std::string_view>>;
        {option.parse_impl(arg)} ->std::same_as<std::optional<typename T::parse_result_type>>;
    };

    template <SingleOption Option>
    constexpr auto parse(Option const & option, int argc, char const * const argv[]) noexcept -> std::optional<typename Option::parse_result_type>;

    #define clp_Opt(type, var)

    template <typename T>
    using get_parse_result_type = typename T::parse_result_type;

    template <SingleOption ... Options>
    struct Compound : private Options...
    {
        constexpr explicit Compound(Options... options) : Options(options)... {}

        struct parse_result_type : public get_parse_result_type<Options>... {};

        template <SingleOption T>
        constexpr T const & access_option() const noexcept
        {
            return static_cast<T const &>(*this);
        }
    };

    template <SingleOption ... Options>
    constexpr auto parse(Compound<Options...> const & parser, int argc, char const * const argv[]) noexcept;

    template <SingleOption A, SingleOption B>         constexpr Compound<A, B> operator | (A a, B b) noexcept;
    template <SingleOption ... A, SingleOption B>     constexpr Compound<A..., B> operator | (Compound<A...> a, B b) noexcept;
    template <SingleOption A, SingleOption ... B>     constexpr Compound<A, B...> operator | (A a, Compound<B...> b) noexcept;
    template <SingleOption ... A, SingleOption ... B> constexpr Compound<A..., B...> operator | (Compound<A...> a, Compound<B...> b) noexcept;

    template <typename T>
    //concept Parser = requires(T const parser, int argc, char const * const * argv) { {clp::parse(parser, argc, argv)} -> std::same_as<std::optional<typename T::parse_result_type>>; };
    concept Parser = true;

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

        template <Parser T>
        constexpr Command<T> const & access_subparser() const noexcept
        {
            return static_cast<Command<T> const &>(*this);
        }
    };

    template <Parser ... Subparsers>
    constexpr auto parse(Commands<Subparsers...> const & commands, int argc, char const * const argv[]) noexcept
        -> std::optional<typename Commands<Subparsers...>::parse_result_type>;

    template <Parser A, Parser B>     constexpr Commands<A, B> operator | (Command<A> a, Command<B> b) noexcept;
    template <Parser ... A, Parser B> constexpr Commands<A..., B> operator | (Commands<A...> a, Command<B> b) noexcept;
    template <Parser A, Parser ... B> constexpr Commands<A, B...> operator | (Command<A> a, Commands<B...> b) noexcept;
    template <Parser ... A, Parser ... B> constexpr Commands<A..., B...> operator | (Commands<A...> a, Commands<B...> b) noexcept;

} // namespace clp

#include "clp.inl"
