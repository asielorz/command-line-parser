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
        template <Parser Next, Parser ... Rest, Parser ... Subparsers>
        constexpr auto parse_impl(Commands<Subparsers...> const & commands, int argc, char const * const argv[]) noexcept
            -> std::optional<typename Commands<Subparsers...>::parse_result_type>
        {
            Command<Next> const & next = commands.access_subparser<Next>();
            if (next.name == argv[0])
            {
                auto result = clp::parse(next.parser, argc - 1, argv + 1);
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

    template <Parser ... Subparsers>
    constexpr auto parse(Commands<Subparsers...> const & commands, int argc, char const * const argv[]) noexcept
        -> std::optional<typename Commands<Subparsers...>::parse_result_type>
    {
        if (argc <= 0)
            return std::nullopt;

        return clp::detail::parse_impl<Subparsers...>(commands, argc, argv);
    }

    template <Parser A, Parser B>
    constexpr Commands<A, B> operator | (Command<A> a, Command<B> b) noexcept
    {
        return Commands<A, B>(a, b);
    }

    template <Parser ... A, Parser B>
    constexpr Commands<A..., B> operator | (Commands<A...> a, Command<B> b) noexcept
    {
        return Commands<A..., B>(a.template access_subparser<A>()..., b);
    }

    template <Parser A, Parser ... B>
    constexpr Commands<A, B...> operator | (Command<A> a, Commands<B...> b) noexcept
    {
        return Commands<A, B...>(a, b.template access_subparser<B>()...);
    }

    template <Parser ... A, Parser ... B>
    constexpr Commands<A..., B...> operator | (Commands<A...> a, Commands<B...> b) noexcept
    {
        return Commands<A..., B...>(a.template access_subparser<A>()..., b.template access_subparser<B>()...);
    }

} // namespace clp