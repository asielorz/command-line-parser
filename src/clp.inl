namespace clp
{

    template <SingleOption Option>
    constexpr auto parse(Option const & option, int argc, char const * const argv[]) noexcept -> std::optional<typename Option::parse_result_type>
    {
        for (int i = 0; i < argc; ++i)
        {
            std::optional<std::string_view> const matched = option.match(argv[i]);
            if (matched)
            {
                if constexpr (HasImplicitValue<Option>)
                    if (matched->empty())
                        return typename Option::parse_result_type{option.implicit_value};

                auto parse_result = option.parse_impl(*matched);

                if constexpr (HasValidationCheck<Option>)
                    if (parse_result && !option.validate(*parse_result))
                        return std::nullopt;

                return parse_result;
            }
        }

        if constexpr (HasDefaultValue<Option>)
            return typename Option::parse_result_type{option.default_value};
        else
            return std::nullopt;
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

    template <SingleOption ... Options>
    constexpr auto parse(Compound<Options...> const & parser, int argc, char const * const argv[]) noexcept
    {
        using parse_result_type = typename Compound<Options...>::parse_result_type;

        return[...opts = parse(parser.access_option<Options>(), argc, argv)]() noexcept -> std::optional<parse_result_type>
        {
            if (!(opts && ...))
                return std::nullopt;
            else
                return parse_result_type{ std::move(*opts)... };
        }();
    }

    template <SingleOption A, SingleOption B>
    constexpr Compound<A, B> operator | (A a, B b) noexcept
    {
        return Compound<A, B>(a, b);
    }

    template <SingleOption ... A, SingleOption B>
    constexpr Compound<A..., B> operator | (Compound<A...> a, B b) noexcept
    {
        return Compound<A..., B>(a.template access_option<A>()..., b);
    }

    template <SingleOption A, SingleOption ... B>
    constexpr Compound<A, B...> operator | (A a, Compound<B...> b) noexcept
    {
        return Compound<A, B...>(a, b.template access_option<B>()...);
    }

    template <SingleOption ... A, SingleOption ... B>
    constexpr Compound<A..., B...> operator | (Compound<A...> a, Compound<B...> b) noexcept
    {
        return Compound<A..., B...>(
            a.template access_option<A>()...,
            b.template access_option<B>()...);
    }

    namespace detail
    {
        template <CommandType Next, CommandType ... Rest, CommandType ... Commands>
        constexpr auto parse_impl(CommandSelector<Commands...> const & commands, int argc, char const * const argv[]) noexcept
            -> std::optional<typename CommandSelector<Commands...>::parse_result_type>
        {
            Next const & next = commands.access_command<Next>();
            if (next.match(argv[0]))
            {
                auto result = next.parse(argc, argv);
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
    constexpr auto parse(CommandSelector<Commands...> const & commands, int argc, char const * const argv[]) noexcept
        -> std::optional<typename CommandSelector<Commands...>::parse_result_type>
    {
        if (argc <= 0)
            return std::nullopt;

        return clp::detail::parse_impl<Commands...>(commands, argc, argv);
    }

    template <Parser P>
    constexpr auto Command<P>::parse(int argc, char const * const argv[]) const noexcept
    { 
        return clp::parse(parser, argc - 1, argv + 1);
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

    template <SingleOption Option>
    std::string to_string(Option const & option) requires HasDescription<Option>
    {
        constexpr int column_width = 40;

        std::string out = option.patterns_to_string();
        out += " <";
        out += option.type_name;
        out += ">";
        while (out.size() < column_width) out.push_back(' ');
        out += option.description;

        if constexpr (HasDefaultValue<Option>)
        {
            out += '\n';
            for (int i = 0; i < column_width; ++i) out.push_back(' ');
            out += "By default: ";
            out += ::to_string(option.default_value);
        }

        if constexpr (HasImplicitValue<Option>)
        {
            out += '\n';
            for (int i = 0; i < column_width; ++i) out.push_back(' ');
            out += "Implicitly: ";
            out += ::to_string(option.implicit_value);
        }

        out += '\n';
        return out;
    }

    template <SingleOption ... Options>
    std::string to_string(Compound<Options...> const & parser)
    {
        return (to_string(parser.template access_option<Options>()) + ...);
    }

} // namespace clp