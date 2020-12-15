namespace dodo
{
    namespace detail
    {
        template <typename ParseResultType, typename T>
        constexpr ParseResultType make_parse_result(T t) noexcept
        {
            using ValueType = typename ParseResultType::value_type;
            return ParseResultType{ ValueType(t) };
        }

        template <typename ... Args>
        Error<std::string> make_error(Args const & ... args)
        {
            std::string result;
            ((result += args), ...);
            return result;
        }

    } // namespace detail

    //*****************************************************************************************************************************************************
    // OptionInterface

    template <typename Base>
    auto OptionInterface<Base>::parse(std::string_view matched_arg) const noexcept -> expected<typename Base::parse_result_type, std::string>
    {
        if constexpr (HasImplicitValue<Base>)
            if (matched_arg.empty())
                return detail::make_parse_result<typename Base::parse_result_type>(this->implicit_value);

        auto parse_result = this->parse_impl(matched_arg);
        if (!parse_result)
            return detail::make_error("Could not convert argument \"", matched_arg, "\" to type ", this->type_name);

        if constexpr (HasValidationCheck<Base>)
        {
            std::optional<std::string_view> const validation_error_message = this->validate(*parse_result);
            if (validation_error_message)
                return detail::make_error(
                    "Validation check failed for option ", this->patterns_to_string(), "with argument \"", matched_arg, "\":\n\t",
                    *validation_error_message);
        }

        return std::move(*parse_result);
    }

    template <typename Base>
    auto OptionInterface<Base>::parse(ArgsView args) const noexcept -> expected<typename Base::parse_result_type, std::string>
    {
        if (args.size() == 0)
        {
            if constexpr (HasDefaultValue<Base>)
                return detail::make_parse_result<typename Base::parse_result_type>(this->default_value);
            else
                return detail::make_error("No matching argument for option ", this->patterns_to_string());
        }
        else if (args.size() == 1)
        {
            std::optional<std::string_view> const matched = this->match(args[0]);
            if (matched)
                return this->parse(*matched);
        }

        return detail::make_error(
            "No matching argument for option ", this->patterns_to_string(), "\n"
            "Unrecognized parameter \"", args[0], '"'
        );
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
            out += dodo::to_string(this->default_value);
        }

        if constexpr (HasImplicitValue<Base>)
        {
            out += '\n';
            for (int i = 0; i < column_width; ++i) out.push_back(' ');
            out += "Implicitly: ";
            out += dodo::to_string(this->implicit_value);
        }

        out += '\n';
        return out;
    }

    #undef dodo_Opt
    #define dodo_Opt(type, var) dodo::OptionInterface(dodo::Option<std::remove_pointer_t<decltype([](){     \
        struct OptionTypeImpl                                                                               \
        {                                                                                                   \
            using value_type = type;                                                                        \
            type var;                                                                                       \
            constexpr type const & _get() const noexcept { return var; }                                    \
        };                                                                                                  \
        return static_cast<OptionTypeImpl *>(nullptr);                                                      \
    }())>>(#type))

    #undef dodo_Flag
    #define dodo_Flag(var) dodo_Opt(bool, var).by_default(false).implicitly(true)

    //*****************************************************************************************************************************************************
    // PositionalArgumentInterface

    template <typename Base>
    auto PositionalArgumentInterface<Base>::parse(std::string_view matched_arg) const noexcept -> expected<typename Base::parse_result_type, std::string>
    {
        auto parse_result = this->parse_impl(matched_arg);
        if (!parse_result)
            return detail::make_error("Could not convert argument \"", matched_arg, "\" to type ", this->type_name);

        if constexpr (HasValidationCheck<Base>)
        {
            std::optional<std::string_view> const validation_error_message = this->validate(*parse_result);
            if (validation_error_message)
                return detail::make_error(
                    "Validation check failed for argument ", this->name, "with argument \"", matched_arg, "\":\n\t",
                    *validation_error_message);
        }

        return std::move(*parse_result);
    }

    template <typename Base>
    auto PositionalArgumentInterface<Base>::parse(ArgsView args) const noexcept -> expected<typename Base::parse_result_type, std::string>
    {
        if (args.size() == 0)
        {
            if constexpr (HasDefaultValue<Base>)
                return detail::make_parse_result<typename Base::parse_result_type>(this->default_value);
            else
                return detail::make_error("Missing argument ", this->name);
        }
        else if (args.size() == 1)
        {
            return this->parse(args[0]);
        }
        else
        {
            return detail::make_error("Too many arguments.");
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

    #undef dodo_Arg
    #define dodo_Arg(type, var, name) dodo::PositionalArgumentInterface(dodo::PositionalArgument<std::remove_pointer_t<decltype([](){   \
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
    using option_parse_result = std::optional<expected<typename Option::parse_result_type, std::string>>;

    template <SingleOption Option>
    bool try_parse_argument(Option const & parser, std::string_view arg, option_parse_result<Option> & result)
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
    void complete_with_default_value([[maybe_unused]] Option const & parser, [[maybe_unused]] option_parse_result<Option> & result)
    {
        if constexpr (HasDefaultValue<Option>)
            if (!result)
                result = option_parse_result<Option>(detail::make_parse_result<typename Option::parse_result_type>(parser.default_value));
    }

    template <SingleOption ... Options>
    auto CompoundOption<Options...>::parse(ArgsView args) const noexcept -> expected<parse_result_type, std::string>
    {
        std::tuple<option_parse_result<Options>...> option_parse_results;

        for (std::string_view const arg : args)
        {
            bool const argument_parsed = (try_parse_argument(
                access_option<Options>(),
                arg,
                std::get<option_parse_result<Options>>(option_parse_results)
            ) || ...);

            if (!argument_parsed)
                return detail::make_error("Unrecognized argument \"", arg, '"');
        }

        (complete_with_default_value(access_option<Options>(), std::get<option_parse_result<Options>>(option_parse_results)), ...);

        // Check that all options were matched.
        if (!(std::get<option_parse_result<Options>>(option_parse_results) && ...))
            return detail::make_error("Unmatched option");

        // Check that no option failed to parse.
        if (!(*std::get<option_parse_result<Options>>(option_parse_results) && ...))
            return detail::make_error("Option failed to parse");

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
    auto CompoundArgument<Arguments...>::parse(ArgsView args) const noexcept -> expected<parse_result_type, std::string>
    {
        if (args.size() > sizeof...(Arguments))
            return detail::make_error("Too many arguments. Provided", std::to_string(args.size()), "arguments. Program expects ", std::to_string(sizeof...(Arguments)));

        auto const results = [this, args]<size_t ... Is>(std::index_sequence<Is...>)
        {
            using Tup = std::tuple<Arguments...>;
            return std::make_tuple(this->template access_argument<std::tuple_element_t<Is, Tup>>().parse(Is >= args.size() ? args.first(0) : args.subspan(Is, 1))...);

        }(std::make_index_sequence<sizeof...(Arguments)>());

        // Ensure that all arguments were parsed.
        if (!(std::get<expected<detail::get_parse_result_type<Arguments>, std::string>>(results) && ...))
            return detail::make_error("Argument failed to parse");

        return parse_result_type{std::move(*std::get<expected<detail::get_parse_result_type<Arguments>, std::string>>(results))...};
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
    auto CompoundParser<Arguments, Options>::parse(ArgsView args) const noexcept -> expected<parse_result_type, std::string>
    {
        auto const first_option = std::find_if(args.begin(), args.end(), [](std::string_view arg) { return arg[0] == '-'; });
        size_t const positional_arg_count = size_t(first_option - args.begin());

        auto parsed_args = Arguments::parse(args.first(positional_arg_count));
        if (!parsed_args)
            return Error(std::move(parsed_args.error()));

        auto opts = Options::parse(args.last(args.size() - positional_arg_count));
        if (!opts)
            return Error(std::move(opts.error()));

        return parse_result_type{std::move(*parsed_args), std::move(*opts)};
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

    template <SingleArgument ... A, SingleOption B>
    constexpr CompoundParser<CompoundArgument<A...>, CompoundOption<B>> operator | (CompoundArgument<A...> a, B b) noexcept
    {
        return CompoundParser<CompoundArgument<A...>, CompoundOption<B>>(a, CompoundOption<B>(b));
    }

    template <SingleArgument A, SingleOption ... B>
    constexpr CompoundParser<CompoundArgument<A>, CompoundOption<B...>> operator | (A a, CompoundOption<B...> b) noexcept
    {
        return CompoundParser<CompoundArgument<A>, CompoundOption<B...>>(CompoundArgument<A>(a), b);
    }

    template <SingleArgument ... A, SingleOption ... B>
    constexpr CompoundParser<CompoundArgument<A...>, CompoundOption<B...>> operator | (CompoundArgument<A...> a, CompoundOption<B...> b) noexcept
    {
        return CompoundParser<CompoundArgument<A...>, CompoundOption<B...>>(a, b);
    }

    template <SingleArgument NewArg, SingleArgument ... PrevArgs, SingleOption ... PrevOpts>
    constexpr auto operator | (NewArg a, CompoundParser<CompoundArgument<PrevArgs...>, CompoundOption<PrevOpts...>> b) noexcept
        -> CompoundParser<CompoundArgument<NewArg, PrevArgs...>, CompoundOption<PrevOpts...>>
    {
        return CompoundParser<CompoundArgument<NewArg, PrevArgs...>, CompoundOption<PrevOpts...>>(a | b.access_arguments(), b.access_options());
    }

    template <SingleArgument ... NewArgs, SingleArgument ... PrevArgs, SingleOption ... PrevOpts>
    constexpr auto operator | (CompoundArgument<NewArgs...> a, CompoundParser<CompoundArgument<PrevArgs...>, CompoundOption<PrevOpts...>> b) noexcept
        -> CompoundParser<CompoundArgument<NewArgs..., PrevArgs...>, CompoundOption<PrevOpts...>>
    {
        return CompoundParser<CompoundArgument<NewArgs..., PrevArgs...>, CompoundOption<PrevOpts...>>(a | b.access_arguments(), b.access_options());
    }

    template <SingleArgument ... A, SingleOption ... PrevOpts, SingleOption NewOpt>
    constexpr auto operator | (CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts...>> a, NewOpt b) noexcept
        -> CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts..., NewOpt>>
    {
        return CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts..., NewOpt>>(a.access_arguments, a.access_options | b);
    }

    template <SingleArgument ... A, SingleOption ... PrevOpts, SingleOption ... NewOpts>
    constexpr auto operator | (CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts...>> a, CompoundOption<NewOpts...> b) noexcept
        -> CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts..., NewOpts...>>
    {
        return CompoundParser<CompoundArgument<A...>, CompoundOption<PrevOpts..., NewOpts...>>(a.access_arguments, a.access_options | b);
    }

    template <typename ... ArgsA, typename ... OptsA, typename ... ArgsB, typename ... OptsB>
    constexpr auto operator | (CompoundParser<CompoundArgument<ArgsA...>, CompoundOption<OptsA...>> a, CompoundParser<CompoundArgument<ArgsB...>, CompoundOption<OptsB...>> b) noexcept
        -> CompoundParser<CompoundArgument<ArgsA..., ArgsB...>, CompoundOption<OptsA..., OptsB...>>
    {
        return CompoundParser<CompoundArgument<ArgsA..., ArgsB...>, CompoundOption<OptsA..., OptsB...>>(
            a.access_arguments() | b.access_arguments(),
            a.access_options() | b.access_options()
            );
    }

    //*****************************************************************************************************************************************************
    // CommandSelector

    namespace detail
    {
        template <CommandType Next, CommandType ... Rest, CommandType ... Commands>
        constexpr auto parse_impl(CommandSelector<Commands...> const & commands, ArgsView args) noexcept
            -> expected<typename CommandSelector<Commands...>::parse_result_type, std::string>
        {
            Next const & next = commands.access_command<Next>();
            if (next.match(args[0]))
            {
                auto result = next.parse_command(args);
                if (!result)
                    return Error(std::move(result.error()));
                else
                    return std::move(*result);
            }
            else
            {
                if constexpr (sizeof...(Rest) > 0)
                    return dodo::detail::parse_impl<Rest...>(commands, args);
                else
                    return detail::make_error("Unrecognized command \"", args[0], '"');
            }
        }
    }

    template <CommandType ... Commands>
    auto CommandSelector<Commands...>::parse(ArgsView args) const noexcept -> expected<parse_result_type, std::string>
    {
        if (args.size() <= 0)
            return detail::make_error("Expected command.");

        return dodo::detail::parse_impl<Commands...>(*this, args);
    }

    template <CommandType ... Commands>
    std::string CommandSelector<Commands...>::to_string(int indentation) const noexcept
    {
        return (access_command<Commands>().to_string(indentation) + ...);
    }

    template <Parser P>
    constexpr auto Command<P>::parse_command(ArgsView args) const noexcept
    {
        return parser.parse(args.last(args.size() - 1));
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
    auto CommandWithSharedOptions<SharedOptions, Commands>::parse(ArgsView args) const noexcept
        -> expected<parse_result_type, std::string>
    {
        auto const it = std::find_if(args.begin(), args.end(), [this](std::string_view arg) { return commands.match(arg); });

        // Command not found.
        if (it == args.end())
            return detail::make_error("Expected command.");

        size_t const arguments_until_command = size_t(it - args.begin());

        auto shared_arguments = shared_options.parse(args.first(arguments_until_command));
        if (!shared_arguments)
            return Error(std::move(shared_arguments.error()));

        auto command = commands.parse(args.last(args.size() - arguments_until_command));
        if (!command)
            return Error(std::move(command.error()));

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
    auto CommandWithImplicitCommand<Commands, ImplicitCommand>::parse(ArgsView args) const noexcept
        -> expected<parse_result_type, std::string>
    {
        if (commands.match(args[0]))
        {
            auto parsed_command = commands.parse(args);
            if (!parsed_command)
                return Error(std::move(parsed_command.error()));
            else
                return std::visit([](auto && x) { return parse_result_type(std::move(x)); }, std::move(*parsed_command));
        }
        else
        {
            auto parsed_implicit_command = implicit_command.parse(args);
            if (!parsed_implicit_command)
                return Error(std::move(parsed_implicit_command.error()));
            else
                return std::move(*parsed_implicit_command);
        }
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

    template <typename T, size_t N>
    struct parse_traits<dodo::constant_range<T, N>>
    {
        static std::string to_string(dodo::constant_range<T, N> const & r)
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

} // namespace dodo
