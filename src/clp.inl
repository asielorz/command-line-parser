namespace clp
{
    namespace detail
    {
        template <typename ParseResultType, typename T>
        constexpr ParseResultType make_parse_result(T t) noexcept
        {
            using ValueType = typename ParseResultType::value_type;
            return ParseResultType{ ValueType(t) };
        }
    } // namespace detail

    //*****************************************************************************************************************************************************
    // OptionInterface

    template <typename Base>
    constexpr auto OptionInterface<Base>::parse(std::string_view matched_arg) const noexcept -> std::optional<typename Base::parse_result_type>
    {
        if constexpr (HasImplicitValue<Base>)
            if (matched_arg.empty())
                return detail::make_parse_result<typename Base::parse_result_type>(this->implicit_value);

        auto parse_result = this->parse_impl(matched_arg);

        if constexpr (HasValidationCheck<Base>)
            if (parse_result && !this->validate(*parse_result))
                return std::nullopt;

        return parse_result;
    }

    template <typename Base>
    constexpr auto OptionInterface<Base>::parse(int argc, char const * const argv[]) const noexcept -> std::optional<typename Base::parse_result_type>
    {
        if (argc == 0)
        {
            if constexpr (HasDefaultValue<Base>)
                return detail::make_parse_result<typename Base::parse_result_type>(this->default_value);
            else
                return std::nullopt;
        }
        else if (argc == 1)
        {
            std::optional<std::string_view> const matched = this->match(argv[0]);
            if (matched)
            {
                if constexpr (HasImplicitValue<Base>)
                    if (matched->empty())
                        return detail::make_parse_result<typename Base::parse_result_type>(this->implicit_value);

                auto parse_result = this->parse_impl(*matched);

                if constexpr (HasValidationCheck<Base>)
                    if (parse_result && !this->validate(*parse_result))
                        return std::nullopt;

                return parse_result;
            }
            else
            {
                return std::nullopt;
            }
        }
        else
        {
            return std::nullopt;
        }
    }

    template <typename Base>
    std::string OptionInterface<Base>::to_string(int indentation) const requires HasDescription<Base>
    {
        constexpr int column_width = 40;

        std::string out;
        out.append(indentation, ' ');
        out += this->patterns_to_string();
        out += " <";
        out += this->hint_text();
        out += ">";
        while (out.size() < column_width) out.push_back(' ');
        out += this->description;

        if constexpr (HasDefaultValue<Base>)
        {
            out += '\n';
            for (int i = 0; i < column_width; ++i) out.push_back(' ');
            out += "By default: ";
            out += ::to_string(this->default_value);
        }

        if constexpr (HasImplicitValue<Base>)
        {
            out += '\n';
            for (int i = 0; i < column_width; ++i) out.push_back(' ');
            out += "Implicitly: ";
            out += ::to_string(this->implicit_value);
        }

        out += '\n';
        return out;
    }

    #undef clp_Opt
    #define clp_Opt(type, var) clp::OptionInterface(clp::Option<std::remove_pointer_t<decltype([](){        \
        struct OptionTypeImpl                                                                               \
        {                                                                                                   \
            using value_type = type;                                                                        \
            type var;                                                                                       \
            constexpr type const & _get() const noexcept { return var; }                                    \
        };                                                                                                  \
        return static_cast<OptionTypeImpl *>(nullptr);                                                      \
    }())>>(#type))

    #undef clp_Flag
    #define clp_Flag(var) clp_Opt(bool, var).default_to(false).implicitly(true)

    //*****************************************************************************************************************************************************
    // PositionalArgumentInterface

    template <typename Base>
    constexpr auto PositionalArgumentInterface<Base>::parse(std::string_view matched_arg) const noexcept -> std::optional<typename Base::parse_result_type>
    {
        auto parse_result = this->parse_impl(matched_arg);

        if constexpr (HasValidationCheck<Base>)
            if (parse_result && !this->validate(*parse_result))
                return std::nullopt;

        return parse_result;
    }

    template <typename Base>
    constexpr auto PositionalArgumentInterface<Base>::parse(int argc, char const * const argv[]) const noexcept -> std::optional<typename Base::parse_result_type>
    {
        if (argc == 0)
        {
            if constexpr (HasDefaultValue<Base>)
                return detail::make_parse_result<typename Base::parse_result_type>(this->default_value);
            else
                return std::nullopt;
        }
        else if (argc == 1)
        {
            return this->parse(argv[0]);
        }
        else
        {
            return std::nullopt;
        }
    }

    template <typename Base>
    std::string PositionalArgumentInterface<Base>::to_string(int indentation) const requires HasDescription<Base>
    {
        constexpr int column_width = 40;

        std::string out;
        out.append(indentation, ' ');
        out += '[';
        out += this->name;
        out += "] <";
        out += this->hint_text();
        out += '>';
        while (out.size() < column_width) out.push_back(' ');
        out += this->description;

        if constexpr (HasDefaultValue<Base>)
        {
            out += '\n';
            for (int i = 0; i < column_width; ++i) out.push_back(' ');
            out += "By default: ";
            out += ::to_string(this->default_value);
        }

        out += '\n';
        return out;
    }

    #undef clp_Arg
    #define clp_Arg(type, var, name) clp::PositionalArgumentInterface(clp::PositionalArgument<std::remove_pointer_t<decltype([](){      \
        struct OptionTypeImpl                                                                                                           \
        {                                                                                                                               \
            using value_type = type;                                                                                                    \
            type var;                                                                                                                   \
            constexpr type const & _get() const noexcept { return var; }                                                                \
        };                                                                                                                              \
        return static_cast<OptionTypeImpl *>(nullptr);                                                                                  \
    }())>>(#type, name))

    //*****************************************************************************************************************************************************
    // CompoundOption

    template <SingleOption Option>
    using option_parse_result = std::optional<std::optional<typename Option::parse_result_type>>;

    template <SingleOption Option>
    constexpr bool try_parse_argument(Option const & parser, std::string_view arg, option_parse_result<Option> & result)
    {
        if (!result)
        {
            std::optional<std::string_view> const matched = parser.match(arg);
            if (matched)
            {
                result = option_parse_result<Option>(parser.parse(*matched));
                return true;
            }
            else
                return false;
        }
        return false;
    }

    template <SingleOption Option>
    constexpr void complete_with_default_value([[maybe_unused]] Option const & parser, [[maybe_unused]] option_parse_result<Option> & result)
    {
        if constexpr (HasDefaultValue<Option>)
            if (!result)
                result.emplace(detail::make_parse_result<typename Option::parse_result_type>(parser.default_value));
    }

    template <SingleOption ... Options>
    constexpr auto CompoundOption<Options...>::parse(int argc, char const * const argv[]) const noexcept -> std::optional<parse_result_type>
    {
        std::tuple<option_parse_result<Options>...> option_parse_results;

        for (int i = 0; i < argc; ++i)
        {
            bool const argument_parsed = (try_parse_argument(
                access_option<Options>(),
                argv[i],
                std::get<option_parse_result<Options>>(option_parse_results)
            ) || ...);

            if (!argument_parsed)
                return std::nullopt;
        }

        (complete_with_default_value(access_option<Options>(), std::get<option_parse_result<Options>>(option_parse_results)), ...);

        // Check that all options were matched.
        if (!(std::get<option_parse_result<Options>>(option_parse_results) && ...))
            return std::nullopt;

        // Check that no option failed to parse.
        if (!(*std::get<option_parse_result<Options>>(option_parse_results) && ...))
            return std::nullopt;

        return parse_result_type{std::move(**std::get<option_parse_result<Options>>(option_parse_results))...};
    }

    template <SingleOption ... Options>
    std::string CompoundOption<Options...>::to_string(int indentation) const
    {
        return (this->template access_option<Options>().to_string(indentation) + ...);
    }

    template <SingleOption A, SingleOption B>
    constexpr CompoundOption<A, B> operator | (A a, B b) noexcept
    {
        return CompoundOption<A, B>(a, b);
    }

    template <SingleOption ... A, SingleOption B>
    constexpr CompoundOption<A..., B> operator | (CompoundOption<A...> a, B b) noexcept
    {
        return CompoundOption<A..., B>(a.template access_option<A>()..., b);
    }

    template <SingleOption A, SingleOption ... B>
    constexpr CompoundOption<A, B...> operator | (A a, CompoundOption<B...> b) noexcept
    {
        return CompoundOption<A, B...>(a, b.template access_option<B>()...);
    }

    template <SingleOption ... A, SingleOption ... B>
    constexpr CompoundOption<A..., B...> operator | (CompoundOption<A...> a, CompoundOption<B...> b) noexcept
    {
        return CompoundOption<A..., B...>(
            a.template access_option<A>()...,
            b.template access_option<B>()...);
    }

    //*****************************************************************************************************************************************************
    // CompoundArgument

    template <SingleArgument ... Arguments>
    constexpr auto CompoundArgument<Arguments...>::parse(int argc, char const * const argv[]) const noexcept -> std::optional<parse_result_type>
    {
        if (argc > sizeof...(Arguments))
            return std::nullopt;

        auto const results = [this, argc, argv]<size_t ... Is>(std::index_sequence<Is...>)
        {
            using Tup = std::tuple<Arguments...>;
            return std::make_tuple(this->template access_argument<std::tuple_element_t<Is, Tup>>().parse(Is >= argc ? 0 : 1, argv + Is)...);

        }(std::make_index_sequence<sizeof...(Arguments)>());

        // Ensure that all arguments were parsed.
        if (!(std::get<std::optional<detail::get_parse_result_type<Arguments>>>(results) && ...))
            return std::nullopt;

        return parse_result_type{std::move(*std::get<std::optional<detail::get_parse_result_type<Arguments>>>(results))...};
    }

    template <SingleArgument ... Arguments>
    std::string CompoundArgument<Arguments...>::to_string(int indentation) const
    {
        return (this->template access_argument<Arguments>().to_string(indentation) + ...);
    }

    template <SingleArgument A, SingleArgument B>
    constexpr CompoundArgument<A, B> operator | (A a, B b) noexcept
    {
        return CompoundArgument<A, B>(a, b);
    }

    template <SingleArgument ... A, SingleArgument B>
    constexpr CompoundArgument<A..., B> operator | (CompoundArgument<A...> a, B b) noexcept
    {
        return CompoundArgument<A..., B>(a.template access_argument<A>()..., b);
    }

    template <SingleArgument A, SingleArgument ... B>
    constexpr CompoundArgument<A, B...> operator | (A a, CompoundArgument<B...> b) noexcept
    {
        return CompoundArgument<A, B...>(a, b.template access_argument<B>()...);
    }

    template <SingleArgument ... A, SingleArgument ... B>
    constexpr CompoundArgument<A..., B...> operator | (CompoundArgument<A...> a, CompoundArgument<B...> b) noexcept
    {
        return CompoundArgument<A..., B...>(
            a.template access_argument<A>()...,
            b.template access_argument<B>()...);
    }

    //*****************************************************************************************************************************************************
    // CompoundParser

    template <instantiation_of<CompoundArgument> Arguments, instantiation_of<CompoundOption> Options>
    constexpr auto CompoundParser<Arguments, Options>::parse(int argc, char const * const argv[]) const noexcept -> std::optional<parse_result_type>
    {
        auto const first_option = std::find_if(argv, argv + argc, [](char const arg[]) { return arg[0] == '-'; });
        int const positional_arg_count = int(first_option - argv);

        auto args = Arguments::parse(positional_arg_count, argv);
        auto opts = Options::parse(argc - positional_arg_count, first_option);

        if (!args || !opts)
            return std::nullopt;

        return parse_result_type{std::move(*args), std::move(*opts)};
    }

    template <instantiation_of<CompoundArgument> Arguments, instantiation_of<CompoundOption> Options>
    std::string CompoundParser<Arguments, Options>::to_string(int indentation) const
    {
        std::string out;

        out.append(indentation, ' ');
        out += "Arguments:\n";
        out += Arguments::to_string(indentation + 2);
        out += '\n';
        out.append(indentation, ' ');
        out += "Options:\n";
        out += Options::to_string(indentation + 2);

        return out;
    }

    template <SingleArgument A, SingleOption B>
    constexpr CompoundParser<CompoundArgument<A>, CompoundOption<B>> operator | (A a, B b) noexcept
    {
        return CompoundParser<CompoundArgument<A>, CompoundOption<B>>(CompoundArgument<A>(a), CompoundOption<B>(b));
    }

    //*****************************************************************************************************************************************************
    // CommandSelector

    namespace detail
    {
        template <CommandType Next, CommandType ... Rest, CommandType ... Commands>
        constexpr auto parse_impl(CommandSelector<Commands...> const & commands, int argc, char const * const argv[]) noexcept
            -> std::optional<typename CommandSelector<Commands...>::parse_result_type>
        {
            Next const & next = commands.access_command<Next>();
            if (next.match(argv[0]))
            {
                auto result = next.parse_command(argc, argv);
                if (!result)
                    return std::nullopt;
                else
                    return std::move(*result);
            }
            else
            {
                if constexpr (sizeof...(Rest) > 0)
                    return clp::detail::parse_impl<Rest...>(commands, argc, argv);
                else
                    return std::nullopt;
            }
        }
    }

    template <CommandType ... Commands>
    constexpr auto CommandSelector<Commands...>::parse(int argc, char const * const argv[]) const noexcept -> std::optional<parse_result_type>
    {
        if (argc <= 0)
            return std::nullopt;

        return clp::detail::parse_impl<Commands...>(*this, argc, argv);
    }

    template <CommandType ... Commands>
    std::string CommandSelector<Commands...>::to_string(int indentation) const noexcept
    {
        return (access_command<Commands>().to_string(indentation) + ...);
    }

    template <Parser P>
    constexpr auto Command<P>::parse_command(int argc, char const * const argv[]) const noexcept
    { 
        return parser.parse(argc - 1, argv + 1);
    }

    template <Parser P>
    std::string Command<P>::to_string(int indentation) const noexcept
    {
        constexpr int column_width = 25;

        std::string out;

        out.append(indentation, ' ');
        out += name;
        while (out.size() < column_width) out.push_back(' ');
        out += description;
        out += '\n';

        return out;
    }

    template <CommandType A, CommandType B>
    constexpr CommandSelector<A, B> operator | (A a, B b) noexcept
    {
        return CommandSelector<A, B>(a, b);
    }

    template <CommandType ... A, CommandType B>
    constexpr CommandSelector<A..., B> operator | (CommandSelector<A...> a, B b) noexcept
    {
        return CommandSelector<A..., B>(a.template access_command<A>()..., b);
    }

    template <CommandType A, CommandType ... B>
    constexpr CommandSelector<A, B...> operator | (A a, CommandSelector<B...> b) noexcept
    {
        return CommandSelector<A, B...>(a, b.template access_command<B>()...);
    }

    template <CommandType ... A, CommandType ... B>
    constexpr CommandSelector<A..., B...> operator | (CommandSelector<A...> a, CommandSelector<B...> b) noexcept
    {
        return CommandSelector<A..., B...>(a.template access_command<A>()..., b.template access_command<B>()...);
    }

    template <Parser SharedOptions, instantiation_of<CommandSelector> Commands>
    constexpr auto CommandWithSharedOptions<SharedOptions, Commands>::parse(int argc, char const * const argv[]) const noexcept -> std::optional<parse_result_type>
    {
        auto const it = std::find_if(argv, argv + argc, [this](char const * arg) { return commands.match(arg); });

        // Command not found.
        if (it == argv + argc)
            return std::nullopt;

        int const arguments_until_command = int(it - argv);

        auto shared_arguments = shared_options.parse(arguments_until_command, argv);
        if (!shared_arguments)
            return std::nullopt;

        auto command = commands.parse(argc - arguments_until_command, it);
        if (!command)
            return std::nullopt;

        return parse_result_type{std::move(*shared_arguments), std::move(*command)};
    }

    template <Parser SharedOptions, instantiation_of<CommandSelector> Commands>
    std::string CommandWithSharedOptions<SharedOptions, Commands>::to_string(int indentation) const noexcept
    {
        std::string out;
        out.append(indentation, ' ');
        out += "Shared options:\n";
        out += shared_options.to_string(indentation + 2);
        out += '\n';
        out.append(indentation, ' ');
        out += "Commands:\n";
        out += commands.to_string(indentation + 2);
        return out;
    }

    template <Parser P, CommandType Command>
    constexpr CommandWithSharedOptions<P, CommandSelector<Command>> operator | (SharedOptions<P> a, Command b) noexcept
    {
        return CommandWithSharedOptions<P, CommandSelector<Command>>(a.parser, CommandSelector<Command>(b));
    }

    template <Parser P, CommandType ... PreviousCommands, CommandType NewCommand>
    constexpr auto operator | (CommandWithSharedOptions<P, CommandSelector<PreviousCommands...>> a, NewCommand b) noexcept
        -> CommandWithSharedOptions<P, CommandSelector<PreviousCommands..., NewCommand>>
    {
        return CommandWithSharedOptions<P, CommandSelector<PreviousCommands..., NewCommand>>(a.shared_options, a.commands | b);
    }

    template <instantiation_of<CommandSelector> Commands, Parser ImplicitCommand>
    constexpr auto CommandWithImplicitCommand<Commands, ImplicitCommand>::parse(int argc, char const * const argv[]) const noexcept -> std::optional<parse_result_type>
    {
        if (commands.match(argv[0]))
        {
            auto parsed_command = commands.parse(argc, argv);
            if (!parsed_command)
                return std::nullopt;
            else
                return std::visit([](auto && x) { return parse_result_type(std::move(x)); }, std::move(*parsed_command));
        }
        else
            return implicit_command.parse(argc, argv);
    }

    template <instantiation_of<CommandSelector> Commands, Parser ImplicitCommand>
    std::string CommandWithImplicitCommand<Commands, ImplicitCommand>::to_string(int indentation) const noexcept
    {
        std::string out;
        out.append(indentation, ' ');
        out += "Commands:\n";
        out += commands.to_string(indentation + 2);
        out += '\n';
        out.append(indentation, ' ');
        out += "Options:\n";
        out += implicit_command.to_string(indentation + 2);
        return out;
    }

    template <CommandType Command, Parser ImplicitCommand>
    constexpr CommandWithImplicitCommand<CommandSelector<Command>, ImplicitCommand> operator | (Command command, ImplicitCommand implicit_command) noexcept
    {
        return CommandWithImplicitCommand<CommandSelector<Command>, ImplicitCommand>(CommandSelector<Command>(command), implicit_command);
    }

    template <instantiation_of<CommandSelector> Commands, Parser ImplicitCommand>
    constexpr CommandWithImplicitCommand<Commands, ImplicitCommand> operator | (Commands commands, ImplicitCommand implicit_command) noexcept
    {
        return CommandWithImplicitCommand<Commands, ImplicitCommand>(commands, implicit_command);
    }

    template <instantiation_of<CommandSelector> Commands, Parser CurrentImplicitCommand, Parser NewImplicitCommand>
    constexpr auto operator | (CommandWithImplicitCommand<Commands, CurrentImplicitCommand> commands, NewImplicitCommand new_implicit_command) noexcept
        -> CommandWithImplicitCommand<Commands, decltype(commands.implicit_command | new_implicit_command)>
    {
        return CommandWithImplicitCommand<Commands, decltype(commands.implicit_command | new_implicit_command)>
            (commands.commands, commands.implicit_command | new_implicit_command);
    }

} // namespace clp

template <typename T, size_t N>
struct parse_traits<clp::constant_range<T, N>>
{
    static std::string to_string(clp::constant_range<T, N> const & r)
    {
        std::string result;

        for (T const & t : r.array)
        {
            result += parse_traits<T>::to_string(t);
            result += ' ';
        }

        // Remove last space.
        if (!result.empty())
            result.pop_back();

        return result;
    }
};
