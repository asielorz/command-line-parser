#pragma once

#include "parse_traits.hh"
#include "expected.hh"
#include <concepts>
#include <type_traits>

namespace clp
{

    template <typename From, typename To>
    concept explicitly_convertible_to = requires(From const from) { static_cast<To>(from); };

    namespace detail
    {
        template <template <typename ...> typename Template, typename ... Args>
        constexpr bool instantiation_of_impl(Template<Args...> const *) noexcept { return true; }
    
        template <template <auto ...> typename Template, auto ... Args>
        constexpr bool instantiation_of_impl(Template<Args...> const *) noexcept { return true; }
    
        template <template <typename ...> typename Template, typename T>
        constexpr bool instantiation_of_impl(T const *) noexcept { return false; }
    
        template <typename T>
        constexpr T * null_pointer_to = static_cast<T *>(nullptr);

        template <typename T, typename U> std::variant<T, U> either_impl(T const &, U const &) noexcept;
        template <typename ... Ts, typename T> std::variant<Ts..., T> either_impl(std::variant<Ts...> const &, T const &) noexcept;
        template <typename T, typename ... Ts> std::variant<T, Ts...> either_impl(T const &, std::variant<Ts...> const &) noexcept;
        template <typename ... Ts, typename ... Us> std::variant<Ts..., Us...> either_impl(std::variant<Ts...> const &, std::variant<Us...> const &) noexcept;
    } // namespace detail

    template <typename T, template <typename ...> typename Template>
    concept instantiation_of = detail::instantiation_of_impl<Template>(detail::null_pointer_to<T>);

    template <typename T, typename U>
    using either = decltype(detail::either_impl(std::declval<T>(), std::declval<U>()));

    template <typename T>
    concept HasValidationCheck = requires(T option, typename T::parse_result_type parse_result) {
        {option.validate(parse_result)} -> std::same_as<std::optional<std::string_view>>;
    };

    template <typename Base, typename Predicate>
    struct WithCheck : public Base
    {
        constexpr explicit WithCheck(Base base, Predicate predicate_, std::string_view error_message_) : Base(base), validation_predicate(predicate_), error_message(error_message_) {}

        constexpr std::optional<std::string_view> validate(typename Base::parse_result_type t) const noexcept
        {
            if constexpr (HasValidationCheck<Base>)
            {
                auto const base_check_error_message = Base::validate(t);
                if (base_check_error_message)
                    return base_check_error_message;
            }

            if (validation_predicate(t._get()))
                return std::nullopt;
            else
                return error_message;
        }

    private:
        Predicate validation_predicate;
        std::string_view error_message;
    };

    template <typename Base, explicitly_convertible_to<typename Base::value_type> T>
    struct WithImplicitValue : public Base
    {
        constexpr explicit WithImplicitValue(Base base, T implicit_value_) noexcept : Base(base), implicit_value(std::move(implicit_value_)) {}

        T implicit_value;
    };

    template <typename T>
    concept HasImplicitValue = requires(T option) {
        {option.implicit_value} -> explicitly_convertible_to<typename T::value_type>;
    };

    template <typename Base, explicitly_convertible_to<typename Base::value_type> T>
    struct WithDefaultValue : public Base
    {
        constexpr explicit WithDefaultValue(Base base, T default_value_) noexcept : Base(base), default_value(std::move(default_value_)) {}

        T default_value;
    };

    template <typename T>
    concept HasDefaultValue = requires(T option) {
        {option.default_value} -> explicitly_convertible_to<typename T::value_type>;
    };

    template <typename Base>
    struct WithDescription : public Base
    {
        std::string_view description;
    };

    template <typename T>
    concept HasDescription = requires(T option) {
        {option.description} -> std::convertible_to<std::string_view>;
    };

    template <typename T>
    concept Pattern = requires (T pattern, std::string_view text) { { pattern.match(text) } -> std::same_as<std::optional<std::string_view>>; };

    template <typename Base>
    struct WithPattern : public Base
    {
        constexpr explicit WithPattern(Base base, std::string_view pattern_) : Base(base), pattern(pattern_) { assert(pattern[0] == '-'); }

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

        std::string patterns_to_string() const
        {
            std::string out;

            if constexpr (Pattern<Base>)
            {
                out = Base::patterns_to_string();
                out += ", ";
            }

            out += pattern;
            return out;
        }

    private:
        std::string_view pattern;
    };

    template <typename T, typename ValueType>
    concept ParserFor = requires(T t, std::string_view text) { {t(text)} -> std::same_as<std::optional<ValueType>>; };

    template <typename Base, ParserFor<typename Base::value_type> ParserFunction>
    struct WithCustomParser : public Base
    {
        constexpr explicit WithCustomParser(Base base, ParserFunction parser_function) noexcept : Base(base), custom_parser(parser_function) {}

        constexpr std::optional<typename Base::parse_result_type> parse_impl(std::string_view argument_text) const noexcept
        {
            std::optional<typename Base::value_type> value = custom_parser(argument_text);
            if (value)
                return typename Base::parse_result_type{std::move(*value)};
            else
                return std::nullopt;
        }

    private:
        ParserFunction custom_parser;
    };

    template <typename Base>
    struct WithCustomHint : public Base
    {
        constexpr explicit WithCustomHint(Base base, std::string_view hint) noexcept : Base(base), custom_hint(hint) {}

        constexpr std::string_view hint_text() const noexcept { return custom_hint; }

    private:
        std::string_view custom_hint;
    };

    template <typename T>
    concept OptionStruct = requires(T a) { { a._get() } -> std::same_as<typename T::value_type const &>; };

    template <OptionStruct T>
    struct Option
    {
        using parse_result_type = T;
        using value_type = typename T::value_type;

        constexpr explicit Option(std::string_view type_name_) noexcept : type_name(type_name_) {}

        constexpr std::optional<T> parse_impl(std::string_view argument_text) const noexcept
        {
            std::optional<value_type> value = parse_traits<value_type>::parse(argument_text);
            if (value)
                return T{std::move(*value)};
            else
                return std::nullopt;
        }

        constexpr std::string_view hint_text() const noexcept { return type_name; }

        std::string_view type_name;
    };

    template <typename T>
    concept SingleOption = requires(T option, std::string_view arg) {
        {option.match(arg)} -> std::same_as<std::optional<std::string_view>>;
        {option.parse_impl(arg)} ->std::same_as<std::optional<typename T::parse_result_type>>;
    };

    #define clp_Opt(type, var)
    #define clp_Flag(var);

    template <typename T, size_t N>
    struct constant_range
    {
        T array[N];

        template <std::constructible_from<T const *, T const *> Range>
        explicit operator Range() const noexcept { return Range(std::begin(array), std::end(array)); }
    };

    template <class T, class... U>
    constant_range(T, U...) -> constant_range<T, 1 + sizeof...(U)>;

    template <typename Base>
    struct OptionInterface : public Base
    {
        explicit constexpr OptionInterface(Base base) noexcept : Base(base) {}

        auto parse(std::string_view matched_arg) const noexcept -> expected<typename Base::parse_result_type, std::string>;
        auto parse(int argc, char const * const argv[]) const noexcept -> expected<typename Base::parse_result_type, std::string>;
        std::string to_string(int indentation = 0) const requires HasDescription<Base>;

        constexpr OptionInterface<WithDescription<Base>> operator () (std::string_view description) const noexcept requires(!HasDescription<Base>)
        {
            return OptionInterface<WithDescription<Base>>(WithDescription<Base>{*this, description});
        }

        constexpr OptionInterface<WithPattern<Base>> operator [] (std::string_view pattern) const noexcept
        {
            return OptionInterface<WithPattern<Base>>(WithPattern<Base>(*this, pattern));
        }

        template <explicitly_convertible_to<typename Base::value_type> T>
        constexpr OptionInterface<WithDefaultValue<Base, T>> default_to(T default_value) const noexcept requires(!HasDefaultValue<Base>)
        {
            return OptionInterface<WithDefaultValue<Base, T>>(WithDefaultValue<Base, T>(*this, std::move(default_value)));
        }

        template <typename ... Ts>
        constexpr auto default_to_range(Ts ... default_values) const noexcept -> decltype(default_to(constant_range{default_values...}))
        {
            return this->default_to(constant_range{default_values...});
        }

        template <explicitly_convertible_to<typename Base::value_type> T>
        constexpr OptionInterface<WithImplicitValue<Base, T>> implicitly(T implicit_value) const noexcept requires(!HasImplicitValue<Base>)
        {
            return OptionInterface<WithImplicitValue<Base, T>>(WithImplicitValue<Base, T>(*this, std::move(implicit_value)));
        }

        template <typename ... Ts>
        constexpr auto implicitly_range(Ts ... default_values) const noexcept -> decltype(implicitly(constant_range{default_values...}))
        {
            return this->implicitly(constant_range{default_values...});
        }

        template <typename Predicate>
        constexpr OptionInterface<WithCheck<Base, Predicate>> check(Predicate predicate, std::string_view error_message) const noexcept
        {
            return OptionInterface<WithCheck<Base, Predicate>>(WithCheck<Base, Predicate>(*this, predicate, error_message));
        }

        template <ParserFor<typename Base::value_type> ParserFunction>
        constexpr OptionInterface<WithCustomParser<Base, ParserFunction>> custom_parser(ParserFunction parser_function) const noexcept
        {
            return OptionInterface<WithCustomParser<Base, ParserFunction>>(WithCustomParser<Base, ParserFunction>(*this, parser_function));
        }

        constexpr OptionInterface<WithCustomHint<Base>> hint(std::string_view custom_hint) const noexcept
        {
            return OptionInterface<WithCustomHint<Base>>(WithCustomHint<Base>(*this, custom_hint));
        }
    };

    template <OptionStruct T>
    struct PositionalArgument
    {
        using parse_result_type = T;
        using value_type = typename T::value_type;

        constexpr explicit PositionalArgument(std::string_view name_, std::string_view type_name_) noexcept : name(name_), type_name(type_name_) {}

        constexpr std::optional<T> parse_impl(std::string_view argument_text) const noexcept
        {
            std::optional<value_type> value = parse_traits<value_type>::parse(argument_text);
            if (value)
                return T{ std::move(*value) };
            else
                return std::nullopt;
        }

        constexpr std::string_view hint_text() const noexcept { return type_name; }

        std::string_view name;
        std::string_view type_name;
    };

    template <typename Base>
    struct PositionalArgumentInterface : public Base
    {
        explicit constexpr PositionalArgumentInterface(Base base) noexcept : Base(base) {}

        auto parse(std::string_view matched_arg) const noexcept -> expected<typename Base::parse_result_type, std::string>;
        auto parse(int argc, char const * const argv[]) const noexcept -> expected<typename Base::parse_result_type, std::string>;
        std::string to_string(int indentation = 0) const requires HasDescription<Base>;

        constexpr PositionalArgumentInterface<WithDescription<Base>> operator () (std::string_view description) const noexcept requires(!HasDescription<Base>)
        {
            return PositionalArgumentInterface<WithDescription<Base>>(WithDescription<Base>{*this, description});
        }

        template <explicitly_convertible_to<typename Base::value_type> T>
        constexpr PositionalArgumentInterface<WithDefaultValue<Base, T>> default_to(T default_value) const noexcept requires(!HasDefaultValue<Base>)
        {
            return PositionalArgumentInterface<WithDefaultValue<Base, T>>(WithDefaultValue<Base, T>(*this, std::move(default_value)));
        }

        template <typename ... Ts>
        constexpr auto default_to_range(Ts ... default_values) const noexcept -> decltype(default_to(constant_range{ default_values... }))
        {
            return this->default_to(constant_range{ default_values... });
        }

        template <typename Predicate>
        constexpr PositionalArgumentInterface<WithCheck<Base, Predicate>> check(Predicate predicate, std::string_view error_message) const noexcept
        {
            return PositionalArgumentInterface<WithCheck<Base, Predicate>>(WithCheck<Base, Predicate>(*this, predicate, error_message));
        }

        template <ParserFor<typename Base::value_type> ParserFunction>
        constexpr PositionalArgumentInterface<WithCustomParser<Base, ParserFunction>> custom_parser(ParserFunction parser_function) const noexcept
        {
            return PositionalArgumentInterface<WithCustomParser<Base, ParserFunction>>(WithCustomParser<Base, ParserFunction>(*this, parser_function));
        }

        constexpr PositionalArgumentInterface<WithCustomHint<Base>> hint(std::string_view custom_hint) const noexcept
        {
            return PositionalArgumentInterface<WithCustomHint<Base>>(WithCustomHint<Base>(*this, custom_hint));
        }
    };
    
    template <typename T>
    concept SingleArgument = instantiation_of<T, PositionalArgumentInterface>;

    namespace detail
    {
        template <typename T>
        using get_parse_result_type = typename T::parse_result_type;
    }

    #define clp_parse_result_type(cli) clp::detail::get_parse_result_type<std::remove_cvref_t<decltype(cli)>>
    #define clp_command_type(cli, i) std::variant_alternative_t<i, clp_parse_result_type(cli)>

    template <SingleOption ... Options>
    struct CompoundOption : private Options...
    {
        constexpr explicit CompoundOption(Options... options) noexcept : Options(options)... {}

        struct parse_result_type : public detail::get_parse_result_type<Options>... {};

        auto parse(int argc, char const * const argv[]) const noexcept -> expected<parse_result_type, std::string>;
        std::string to_string(int indentation = 0) const;

        template <SingleOption T>
        constexpr T const & access_option() const noexcept
        {
            return static_cast<T const &>(*this);
        }
    };

    template <SingleOption A, SingleOption B>         constexpr CompoundOption<A, B> operator | (A a, B b) noexcept;
    template <SingleOption ... A, SingleOption B>     constexpr CompoundOption<A..., B> operator | (CompoundOption<A...> a, B b) noexcept;
    template <SingleOption A, SingleOption ... B>     constexpr CompoundOption<A, B...> operator | (A a, CompoundOption<B...> b) noexcept;
    template <SingleOption ... A, SingleOption ... B> constexpr CompoundOption<A..., B...> operator | (CompoundOption<A...> a, CompoundOption<B...> b) noexcept;

    template <SingleArgument ... Arguments>
    struct CompoundArgument : private Arguments...
    {
        constexpr explicit CompoundArgument(Arguments... args) noexcept : Arguments(args)... {}

        struct parse_result_type : public detail::get_parse_result_type<Arguments>... {};

        auto parse(int argc, char const * const argv[]) const noexcept -> expected<parse_result_type, std::string>;
        std::string to_string(int indentation = 0) const;

        template <SingleArgument T>
        constexpr T const & access_argument() const noexcept
        {
            return static_cast<T const &>(*this);
        }
    };

    template <SingleArgument A, SingleArgument B>         constexpr CompoundArgument<A, B> operator | (A a, B b) noexcept;
    template <SingleArgument ... A, SingleArgument B>     constexpr CompoundArgument<A..., B> operator | (CompoundArgument<A...> a, B b) noexcept;
    template <SingleArgument A, SingleArgument ... B>     constexpr CompoundArgument<A, B...> operator | (A a, CompoundArgument<B...> b) noexcept;
    template <SingleArgument ... A, SingleArgument ... B> constexpr CompoundArgument<A..., B...> operator | (CompoundArgument<A...> a, CompoundArgument<B...> b) noexcept;

    template <instantiation_of<CompoundArgument> Arguments, instantiation_of<CompoundOption> Options>
    struct CompoundParser : private Arguments, private Options
    {
        constexpr explicit CompoundParser(Arguments args, Options opts) noexcept : Arguments(args), Options(opts) {}

        struct parse_result_type : public detail::get_parse_result_type<Arguments>, public detail::get_parse_result_type<Options> {};

        auto parse(int argc, char const * const argv[]) const noexcept -> expected<parse_result_type, std::string>;
        std::string to_string(int indentation = 0) const;

        constexpr Options const & access_options() const noexcept
        {
            return *this;
        }

        constexpr Arguments const & access_arguments() const noexcept
        {
            return *this;
        }
    };

    template <SingleArgument A, SingleOption B> 
    constexpr CompoundParser<CompoundArgument<A>, CompoundOption<B>> operator | (A a, B b) noexcept;

    template <SingleArgument ... A, SingleOption B>
    constexpr CompoundParser<CompoundArgument<A...>, CompoundOption<B>> operator | (CompoundArgument<A...> a, B b) noexcept;

    template <SingleArgument A, SingleOption ... B>
    constexpr CompoundParser<CompoundArgument<A>, CompoundOption<B...>> operator | (A a, CompoundOption<B...> b) noexcept;

    template <SingleArgument ... A, SingleOption ... B>
    constexpr CompoundParser<CompoundArgument<A...>, CompoundOption<B...>> operator | (CompoundArgument<A...> a, CompoundOption<B...> b) noexcept;

    template <SingleArgument NewArg, SingleArgument ... PrevArgs, SingleOption ... PrevOpts>
    constexpr auto operator | (NewArg a, CompoundParser<CompoundArgument<PrevArgs...>, CompoundOption<PrevOpts...>> b) noexcept
        -> CompoundParser<CompoundArgument<NewArg, PrevArgs...>, CompoundOption<PrevOpts...>>;

    template <SingleArgument ... NewArgs, SingleArgument ... PrevArgs, SingleOption ... PrevOpts>
    constexpr auto operator | (CompoundArgument<NewArgs...> a, CompoundParser<CompoundArgument<PrevArgs...>, CompoundOption<PrevOpts...>> b) noexcept
        -> CompoundParser<CompoundArgument<NewArgs..., PrevArgs...>, CompoundOption<PrevOpts...>>;

    template <SingleArgument ... A, SingleOption ... PrevOpts, SingleOption NewOpt>
    constexpr auto operator | (CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts...>> a, NewOpt b) noexcept
        -> CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts..., NewOpt>>;

    template <SingleArgument ... A, SingleOption ... PrevOpts, SingleOption ... NewOpts>
    constexpr auto operator | (CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts...>> a, CompoundOption<NewOpts...> b) noexcept
        -> CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts..., NewOpts...>>;

    template <typename ... ArgsA, typename ... OptsA, typename ... ArgsB, typename ... OptsB>
    constexpr auto operator | (CompoundParser<CompoundArgument<ArgsA...>, CompoundOption<OptsA...>> a, CompoundParser<CompoundArgument<ArgsB...>, CompoundOption<OptsB...>> b) noexcept
        -> CompoundParser<CompoundArgument<ArgsA..., ArgsB...>, CompoundOption<OptsA..., OptsB...>>;

    template <typename T>
    concept Parser = requires(T const parser, int argc, char const * const * argv) { 
        {parser.parse(argc, argv)} -> std::same_as<expected<typename T::parse_result_type, std::string>>; 
    };

    template <Parser P>
    struct Command
    {
        using parse_result_type = typename P::parse_result_type;

        explicit constexpr Command(std::string_view name_, std::string_view description_, P parser_) noexcept 
            : name(name_)
            , parser(parser_)
            , description(description_)
        {}

        constexpr bool match(std::string_view text) const noexcept { return text == name; }
        constexpr auto parse_command(int argc, char const * const argv[]) const noexcept;
        std::string to_string(int indentation) const noexcept;

        std::string_view name;
        std::string_view description;
        P parser;
    };

    template <typename T>
    concept CommandType = requires(T t, std::string_view text, int argc, char const * const * argv, int indentation) 
    {
        {t.match(text)} -> std::same_as<bool>;
        {t.parse_command(argc, argv)} -> std::same_as<expected<typename T::parse_result_type, std::string>>;
        {t.to_string(indentation)} -> std::same_as<std::string>;
    };

    template <CommandType ... Commands>
    struct CommandSelector : private Commands...
    {
        using parse_result_type = std::variant<detail::get_parse_result_type<Commands>...>;

        constexpr explicit CommandSelector(Commands... commands) noexcept : Commands(commands)... {}

        auto parse(int argc, char const * const argv[]) const noexcept -> expected<parse_result_type, std::string>;

        constexpr bool match(std::string_view text) const noexcept { return (access_command<Commands>().match(text) || ...); }

        std::string to_string(int indentation = 0) const noexcept;

        template <CommandType C>
        constexpr C const & access_command() const noexcept
        {
            return static_cast<C const &>(*this);
        }
    };

    template <CommandType A, CommandType B>     constexpr CommandSelector<A, B> operator | (A a, B b) noexcept;
    template <CommandType ... A, CommandType B> constexpr CommandSelector<A..., B> operator | (CommandSelector<A...> a, B b) noexcept;
    template <CommandType A, CommandType ... B> constexpr CommandSelector<A, B...> operator | (A a, CommandSelector<B...> b) noexcept;
    template <CommandType ... A, CommandType ... B> constexpr CommandSelector<A..., B...> operator | (CommandSelector<A...> a, CommandSelector<B...> b) noexcept;

    template <Parser SharedOptions, instantiation_of<CommandSelector> Commands>
    struct CommandWithSharedOptions
    {
        constexpr explicit CommandWithSharedOptions(SharedOptions shared_options_, Commands commands_) noexcept
            : shared_options(std::move(shared_options_))
            , commands(std::move(commands_))
        {}

        struct parse_result_type
        {
            typename SharedOptions::parse_result_type shared_arguments;
            typename Commands::parse_result_type command;
        };

        auto parse(int argc, char const * const argv[]) const noexcept -> expected<parse_result_type, std::string>;
        std::string to_string(int indentation = 0) const noexcept;

        SharedOptions shared_options;
        Commands commands;
    };

    template <Parser P>
    struct SharedOptions
    {
        constexpr explicit SharedOptions(P parser_) noexcept : parser(std::move(parser_)) {}
        P parser;
    };

    template <Parser P, CommandType Command>
    constexpr CommandWithSharedOptions<P, CommandSelector<Command>> operator | (SharedOptions<P> a, Command b) noexcept;

    template <Parser P, CommandType ... PreviousCommands, CommandType NewCommand>
    constexpr auto operator | (CommandWithSharedOptions<P, CommandSelector<PreviousCommands...>> a, NewCommand b) noexcept
        -> CommandWithSharedOptions<P, CommandSelector<PreviousCommands..., NewCommand>>;

    template <instantiation_of<CommandSelector> Commands, Parser ImplicitCommand>
    struct CommandWithImplicitCommand
    {
        constexpr explicit CommandWithImplicitCommand(Commands commands_, ImplicitCommand implicit_command_) noexcept
            : commands(std::move(commands_))
            , implicit_command(std::move(implicit_command_))
        {}

        using parse_result_type = either<typename Commands::parse_result_type, typename ImplicitCommand::parse_result_type>;

        auto parse(int argc, char const * const argv[]) const noexcept -> expected<parse_result_type, std::string>;
        std::string to_string(int indentation = 0) const noexcept;

        Commands commands;
        ImplicitCommand implicit_command;
    };

    template <CommandType Command, Parser ImplicitCommand>
    constexpr CommandWithImplicitCommand<CommandSelector<Command>, ImplicitCommand> operator | (Command command, ImplicitCommand implicit_command) noexcept;

    template <instantiation_of<CommandSelector> Commands, Parser ImplicitCommand>
    constexpr CommandWithImplicitCommand<Commands, ImplicitCommand> operator | (Commands commands, ImplicitCommand implicit_command) noexcept;

    template <instantiation_of<CommandSelector> Commands, Parser CurrentImplicitCommand, Parser NewImplicitCommand>
    constexpr auto operator | (CommandWithImplicitCommand<Commands, CurrentImplicitCommand> commands, NewImplicitCommand new_implicit_command) noexcept
        -> CommandWithImplicitCommand<Commands, decltype(commands.implicit_command | new_implicit_command)>;

} // namespace clp

#include "clp.inl"
