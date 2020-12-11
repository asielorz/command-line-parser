#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"

#include "clp.hh"
#include <typeinfo>

using namespace std::literals;

namespace tests
{
    template <clp::Parser P>
    constexpr auto parse(P parser, std::initializer_list<char const *> args) noexcept
    {
        int const argc = int(args.size());
        auto const argv = std::data(args);
        return parser.parse(argc, argv);
    }

    template <typename T>
    bool are_equal(std::vector<T> const & v, std::initializer_list<T> ilist) noexcept
    {
        return std::equal(v.begin(), v.end(), ilist.begin(), ilist.end());
    }

    struct ShowHelp {};

    struct Help
    {
        using parse_result_type = ShowHelp;

        constexpr bool match(std::string_view text) const noexcept { return text == "--help" || text == "-h" || text == "-?" || text == "help"; }
        constexpr std::optional<ShowHelp> parse_command(int argc, char const * const argv[]) const noexcept { static_cast<void>(argc, argv); return ShowHelp(); }
        std::string to_string(int indentation) const noexcept
        {
            std::string result;
            result.append(indentation, ' ');
            result += "help, --help, -h, -?";
            while (result.size() < 25) result += ' ';
            result += "Show help about the program or a specific command.\n";
            return result;
        }
    };
}

int main()
{
    int const result = Catch::Session().run();
    if (result != 0)
        system("pause");
}

TEST_CASE("Single argument CLI")
{
    constexpr auto cli =
        clp_Opt(int, width)
            ["-w"]
            ("Width of the screen in pixels");

    SECTION("Found")
    {
        auto const options = tests::parse(cli, {"-w=1920"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 1920);
    }
    SECTION("Not found")
    {
        auto const options = tests::parse(cli, {"-f=1920"});

        REQUIRE(!options.has_value());
    }
    SECTION("Fail to parse")
    {
        auto const options = tests::parse(cli, {"-w=foo"}); // Cannot parse an int from "foo"

        REQUIRE(!options.has_value());
    }
    SECTION("Correctly parsing a negative integer")
    {
        auto const options = tests::parse(cli, { "-w=-100" });

        REQUIRE(options.has_value());
        REQUIRE(options->width == -100);
    }
}

TEST_CASE("Multiple patterns for an option")
{
    constexpr auto cli =
            clp_Opt(int, width)
            ["-w"]["--width"]
            ("Width of the screen in pixels");

    SECTION("Found -w")
    {
        auto const options = tests::parse(cli, {"-w=10"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 10);
    }
    SECTION("Found --width")
    {
        auto const options = tests::parse(cli, { "--width=-56" });

        REQUIRE(options.has_value());
        REQUIRE(options->width == -56);
    }
    SECTION("Not found")
    {
        auto const options = tests::parse(cli, {"-f=1920"});

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("Default value for a pattern")
{
    constexpr auto cli =
        clp_Opt(int, width)
            ["-w"]
            ("Width of the screen in pixels")
            .default_to(1920);

    SECTION("Found")
    {
        auto const options = tests::parse(cli, {"-w=10"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 10);
    }
    SECTION("Not found")
    {
        auto const options = tests::parse(cli, {});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 1920); // Default value used because another value was not found
    }
}

TEST_CASE("Checks on a pattern that can make it fail even when found")
{
    constexpr auto cli =
        clp_Opt(int, width)
            ["-w"]
            ("Width of the screen in pixels")
            .check([](int width) { return width > 0; }, "Width cannot be negative.");

    SECTION("Found")
    {
        auto const options = tests::parse(cli, {"-w=10"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 10);
    }
    SECTION("Not found")
    {
        auto const options = tests::parse(cli, {"-f=50"});

        REQUIRE(!options.has_value());
    }
    SECTION("Found but not satisfying condition")
    {
        auto const options = tests::parse(cli, {"-w=0"});

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("Multiple checks")
{
    constexpr auto cli =
        clp_Opt(int, width)
            ["-w"]
            ("Width of the screen in pixels")
            .check([](int width) { return width > 0; }, "Width cannot be negative.")
            .check([](int width) { return width % 2 == 0; }, "Width must be even.");

    SECTION("Found")
    {
        auto const options = tests::parse(cli, {"-w=10"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 10);
    }
    SECTION("Negative")
    {
        auto const options = tests::parse(cli, {"-w=-30"});

        REQUIRE(!options.has_value());
    }
    SECTION("Odd")
    {
        auto const options = tests::parse(cli, {"-w=15"});

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("Combining two options in a compound parser")
{
    constexpr auto cli =
        clp_Opt(int, width)
            ["-w"]["--width"]
            ("Width of the screen in pixels")
        | clp_Opt(int, height)
            ["-h"]["--height"]
            ("Height of the screen in pixels");

    SECTION("Both found with short name")
    {
        auto const options = tests::parse(cli, {"-w=30", "-h=20"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 30);
        REQUIRE(options->height == 20);
    }
    SECTION("Both found with long name")
    {
        auto const options = tests::parse(cli, { "--width=30", "--height=20" });

        REQUIRE(options.has_value());
        REQUIRE(options->width == 30);
        REQUIRE(options->height == 20);
    }
    SECTION("One long one short")
    {
        auto const options = tests::parse(cli, {"--width=30", "-h=20"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 30);
        REQUIRE(options->height == 20);
    }
    SECTION("Width missing")
    {
        auto const options = tests::parse(cli, {"-h=20"});

        REQUIRE(!options.has_value());
    }
    SECTION("Height missing")
    {
        auto const options = tests::parse(cli, {"-w=30"});

        REQUIRE(!options.has_value());
    }
    SECTION("Both missing")
    {
        auto const options = tests::parse(cli, { "-foo=true" });

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("Combining three options in a compound parser where one is defaulted")
{
    constexpr auto cli =
        clp_Opt(int, width)
            ["-w"]["--width"]
            ("Width of the screen in pixels")
        | clp_Opt(int, height)
            ["-h"]["--height"]
            ("Height of the screen in pixels")
        | clp_Opt(bool, fullscreen)
            ["--fullscreen"]
            ("Whether or not the program should start in fullscreen")
            .default_to(false);

    SECTION("Fullscreen missing")
    {
        auto const options = tests::parse(cli, {"-w=30", "-h=20"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 30);
        REQUIRE(options->height == 20);
        REQUIRE(options->fullscreen == false);
    }
    SECTION("All found")
    {
        auto const options = tests::parse(cli, {"-w=30", "-h=20", "--fullscreen=true"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 30);
        REQUIRE(options->height == 20);
        REQUIRE(options->fullscreen == true);
    }
    SECTION("Width missing")
    {
        auto const options = tests::parse(cli, {"-h=20", "--fullscreen=true"});

        REQUIRE(!options.has_value());
    }
    SECTION("Height missing")
    {
        auto const options = tests::parse(cli, {"-w=30", "--fullscreen=true"});

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("Defaulted value fails to parse")
{
    constexpr auto cli =
        clp_Opt(int, width)
            ["-w"]
            ("Width of the screen in pixels")
            .default_to(1920);

    auto const options = tests::parse(cli, {"-w=foo"});

    REQUIRE(!options.has_value());
}

TEST_CASE("Combining several parsers in commands")
{
    using clp::Command;

    constexpr auto cli = 
        Command("open-window", "",
            clp_Opt(int, width)["-w"]["--width"]("Width of the screen") |
            clp_Opt(int, height)["-h"]["--height"]("Height of the screen")
        )
        | Command("fetch-url", "",
            clp_Opt(std::string, url)["--url"]("Url to fetch") |
            clp_Opt(int, max_attempts)["--max-attempts"]("Maximum number of attempts before failing") |
            clp_Opt(float, timeout)["--timeout"]("Time to wait for response before failing the attempt").default_to(10.0f)
        );
        
    SECTION("First command")
    {
        auto const options = tests::parse(cli, {"open-window", "-w=1920", "-h=1080"});

        REQUIRE(options.has_value());
        REQUIRE(options->index() == 0);

        auto const window_options = std::get<0>(*options);
        REQUIRE(window_options.width == 1920);
        REQUIRE(window_options.height == 1080);
    }
    SECTION("Second command")
    {
        auto const options = tests::parse(cli, {"fetch-url", "--url=www.google.com", "--max-attempts=15"});

        REQUIRE(options.has_value());
        REQUIRE(options->index() == 1);

        auto const url_options = std::get<1>(*options);
        REQUIRE(url_options.url == "www.google.com");
        REQUIRE(url_options.max_attempts == 15);
        REQUIRE(url_options.timeout == 10.0f);
    }
    SECTION("Unrecognized command")
    {
        auto const options = tests::parse(cli, {"commit", "-m=foo"});

        REQUIRE(!options.has_value());
    }
    SECTION("Command found but fails to parse")
    {
        auto const options = tests::parse(cli, {"fetch-url", "-w=1920", "-h=1080"});

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("Implicit value allows for setting a value implicitly to an option if the option is mentioned but no value is assigned")
{
    constexpr auto cli = clp_Opt(bool, some_flag)["--flag"]
        ("Example boolean flag that defaults to false but is implicitly true when mentioned.")
        .default_to(false)
        .implicitly(true);

    SECTION("Not found. Default, so false")
    {
        auto const options = tests::parse(cli, {});

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == false);
    }
    SECTION("Mentioned but no value assigned. Implicit value, so true")
    {
        auto const options = tests::parse(cli, {"--flag"});

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == true);
    }
    SECTION("Explicitly true")
    {
        auto const options = tests::parse(cli, {"--flag=true"});

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == true);
    }
    SECTION("Explicitly false")
    {
        auto const options = tests::parse(cli, {"--flag=false"});

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == false);
    }
    SECTION("Fail to parse")
    {
        auto const options = tests::parse(cli, { "--flag=quux" });

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("Printing help without word wrap and without commands")
{
    constexpr auto cli = clp_Opt(int, width)["-w"]["--width"]
            ("Width of the screen in pixels.")
            .default_to(1920)
        | clp_Opt(int, height)["-h"]["--height"]
            ("Height of the screen in pixels.")
            .default_to(1080)
        | clp_Opt(bool, fullscreen)["--fullscreen"]
            ("Whether to start the application in fullscreen or not.")
            .default_to(false)
            .implicitly(true)
        | clp_Opt(std::string, starting_level) ["--starting-level"]
            ("Level to open in the editor.");

    std::string const str = cli.to_string();

    constexpr auto expected =
        "-w, --width <int>                       Width of the screen in pixels.\n"
        "                                        By default: 1920\n"
        "-h, --height <int>                      Height of the screen in pixels.\n"
        "                                        By default: 1080\n"
        "--fullscreen <bool>                     Whether to start the application in fullscreen or not.\n"
        "                                        By default: false\n"
        "                                        Implicitly: true\n"
        "--starting-level <std::string>          Level to open in the editor.\n"
    ;

    REQUIRE(str == expected);
}

template <typename ... Ts>
struct overload : public Ts...
{
    constexpr explicit overload(Ts ... ts) noexcept : Ts(ts)... {}
    using Ts::operator()...;
};

TEST_CASE("Help command creates a command that matches --help and indicates the user code that it should print the help")
{
    constexpr auto cli = 
        clp::Command("open-window", "",
            clp_Opt(int, width)["-w"]["--width"]("Width of the screen") |
            clp_Opt(int, height)["-h"]["--height"]("Height of the screen")
        )
        | clp::Command("fetch-url", "",
            clp_Opt(std::string, url)["--url"]("Url to fetch") |
            clp_Opt(int, max_attempts)["--max-attempts"]("Maximum number of attempts before failing") |
            clp_Opt(float, timeout)["--timeout"]("Time to wait for response before failing the attempt").default_to(10.0f)
        )
        | tests::Help();

    constexpr auto visitor = overload(
        [](tests::ShowHelp) { return true; },
        [](auto const &) { return false; }
    );

    SECTION("Match --help")
    {
        auto const options = tests::parse(cli, {"--help"});

        REQUIRE(options.has_value());
        REQUIRE(std::visit(visitor, *options));
    }
    SECTION("Match -h")
    {
        auto const options = tests::parse(cli, {"-h"});

        REQUIRE(options.has_value());
        REQUIRE(std::visit(visitor, *options));
    }
    SECTION("Match -?")
    {
        auto const options = tests::parse(cli, {"-?"});

        REQUIRE(options.has_value());
        REQUIRE(std::visit(visitor, *options));
    }
    SECTION("Match something else")
    {
        auto const options = tests::parse(cli, {"open-window", "-w=10", "-h=6"});

        REQUIRE(options.has_value());
        REQUIRE(!std::visit(visitor, *options));
    }
    SECTION("Match nothing")
    {
        auto const options = tests::parse(cli, {"make-snafucated"});

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("A flag is a boolean option that is by default false and implicitly true")
{
    constexpr auto cli = clp_Flag(some_flag)["--flag"]("Example flag.");

    SECTION("Not found. Default, so false")
    {
        auto const options = tests::parse(cli, {});

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == false);
    }
    SECTION("Mentioned but no value assigned. Implicit value, so true")
    {
        auto const options = tests::parse(cli, { "--flag" });

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == true);
    }
    SECTION("Explicitly true")
    {
        auto const options = tests::parse(cli, { "--flag=true" });

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == true);
    }
    SECTION("Explicitly false")
    {
        auto const options = tests::parse(cli, { "--flag=false" });

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == false);
    }
    SECTION("Fail to parse")
    {
        auto const options = tests::parse(cli, { "--flag=quux" });

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("A custom parser may be given to an option")
{
    constexpr auto on_off_boolean_parser = [](std::string_view text) noexcept -> std::optional<bool>
    {
        if (text == "on")
            return true;
        else if (text == "off")
            return false;
        else
            return std::nullopt;
    };

    constexpr auto cli = clp_Flag(some_flag)["--flag"]("Example flag.")
        .custom_parser(on_off_boolean_parser);

    SECTION("on -> true")
    {
        auto const options = tests::parse(cli, {"--flag=on"});

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == true);
    }
    SECTION("off -> false")
    {
        auto const options = tests::parse(cli, {"--flag=off"});

        REQUIRE(options.has_value());
        REQUIRE(options->some_flag == false);
    }
    SECTION("Fail to parse. Default parser is not used so true is not a valid parser for bool anymore.")
    {
        auto const options = tests::parse(cli, {"--flag=true"});

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("parse may be called at compile time")
{
    constexpr auto cli = clp_Flag(some_flag)["--flag"]("Example flag.")
        | clp_Opt(std::string_view, some_string)["--str"]("Some text");

    constexpr auto options = tests::parse(cli, {"--flag", "--str=foo"});
    STATIC_REQUIRE(options.has_value());
    STATIC_REQUIRE(options->some_flag == true);
    STATIC_REQUIRE(options->some_string == "foo");
}

TEST_CASE("A custom hint may be given to a variable when the type name may not be readable enough.")
{
    constexpr auto cli = clp_Opt(int, width)["-w"]["--width"]
            ("Width of the screen in pixels.")
            .default_to(1920)
        | clp_Opt(int, height)["-h"]["--height"]
            ("Height of the screen in pixels.")
            .default_to(1080)
        | clp_Opt(bool, fullscreen)["--fullscreen"]
            ("Whether to start the application in fullscreen or not.")
            .default_to(false)
            .implicitly(true)
        | clp_Opt(std::string, starting_level) ["--starting-level"]
            ("Level to open in the editor.")
            .hint("level-name");

    std::string const str = cli.to_string();

    constexpr auto expected =
        "-w, --width <int>                       Width of the screen in pixels.\n"
        "                                        By default: 1920\n"
        "-h, --height <int>                      Height of the screen in pixels.\n"
        "                                        By default: 1080\n"
        "--fullscreen <bool>                     Whether to start the application in fullscreen or not.\n"
        "                                        By default: false\n"
        "                                        Implicitly: true\n"
        "--starting-level <level-name>           Level to open in the editor.\n"
    ;

    REQUIRE(str == expected);
}

TEST_CASE("Implicit and default values can be of types different to the value type, allowing for constexpr parsers for std::string by using string views")
{
    constexpr auto cli = clp_Opt(std::string, starting_level) ["--starting-level"]
            ("Level to open in the editor.")
            .default_to("new-level"sv)
            .implicitly("main-world"sv)
            .hint("level-name");

    SECTION("Correctly parsed")
    {
        auto const options = tests::parse(cli, {"--starting-level=1-1"});

        REQUIRE(options.has_value());
        REQUIRE(options->starting_level == "1-1");
    }
    SECTION("Default")
    {
        auto const options = tests::parse(cli, {});

        REQUIRE(options.has_value());
        REQUIRE(options->starting_level == "new-level");
    }
    SECTION("Implicit")
    {
        auto const options = tests::parse(cli, {"--starting-level"});

        REQUIRE(options.has_value());
        REQUIRE(options->starting_level == "main-world");
    }
}

template <typename T>
struct v
{
    explicit constexpr v(std::initializer_list<T> ilist_) noexcept : ilist(ilist_) {}

    std::initializer_list<T> ilist;
};

template <typename T>
bool operator == (std::vector<T> const & v1, v<T> v2) noexcept
{
    return std::equal(v1.begin(), v1.end(), v2.ilist.begin(), v2.ilist.end());
}

template <typename T>
std::ostream & operator << (std::ostream & os, v<T> v)
{
    os << "{ ";
    for (T const & t : v.ilist)
        os << t << ' ';
    os << '}';
    return os;
}

TEST_CASE("Option of vector type")
{
    constexpr auto cli = clp_Opt(std::vector<int>, values) ["--values"]
            ("Some test integers.")
            .default_to_range(1, 2, 3)
            .implicitly_range(0, 5, 4, 5);

    SECTION("Parse correctly")
    {
        auto const options = tests::parse(cli, {"--values=4 5 6"});

        REQUIRE(options.has_value());
        REQUIRE(options->values == v{4, 5, 6});
    }
    SECTION("Default")
    {
        auto const options = tests::parse(cli, {});

        REQUIRE(options.has_value());
        REQUIRE(options->values == v{1, 2, 3});
    }
    SECTION("Implicit")
    {
        auto const options = tests::parse(cli, {"--values"});

        REQUIRE(options.has_value());
        REQUIRE(options->values == v{0, 5, 4, 5});
    }
}

TEST_CASE("Printing parsers of vectors")
{
    constexpr auto cli = clp_Opt(std::vector<int>, values) ["--values"]
            ("Some test integers.")
            .default_to_range(1, 2, 3)
            .implicitly_range(0, 5, 4, 5);

    std::string const str = cli.to_string();
    
    constexpr auto expected =
        "--values <std::vector<int>>             Some test integers.\n"
        "                                        By default: 1 2 3\n"
        "                                        Implicitly: 0 5 4 5\n"
        ;

    REQUIRE(str == expected);
}

TEST_CASE("Unrecognized arguments are an error")
{
    constexpr auto cli =
        clp_Opt(int, width) ["-w"]["--width"]
        | clp_Opt(int, height) ["-h"]["--height"]
        | clp_Opt(bool, fullscreen) ["--fullscreen"];

    auto const options = tests::parse(cli, {"-w=10", "-h=6", "--fullscreen=true", "--unrecognized=5"});

    REQUIRE(!options.has_value());
}

TEST_CASE("instantiation_of is a concept that checks if a type is an instantiation of a template")
{
    STATIC_REQUIRE(clp::instantiation_of<std::vector<int>, std::vector>);
    STATIC_REQUIRE(!clp::instantiation_of<int, std::vector>);
    STATIC_REQUIRE(clp::instantiation_of<std::string, std::basic_string>);
}

TEST_CASE("Commands with shared options")
{
    constexpr auto cli =
        clp::SharedOptions(
            clp_Opt(std::string, root_path)["--root-path"]
                .default_to("."sv)
            | clp_Flag(dry_run) ["--dry-run"]
        ) 
        | clp::Command("open-window", "",
            clp_Opt(int, width)["-w"]["--width"]("Width of the screen") |
            clp_Opt(int, height)["-h"]["--height"]("Height of the screen")
        )
        | clp::Command("fetch-url", "",
            clp_Opt(std::string, url)["--url"]("Url to fetch") |
            clp_Opt(int, max_attempts)["--max-attempts"]("Maximum number of attempts before failing") |
            clp_Opt(float, timeout)["--timeout"]("Time to wait for response before failing the attempt").default_to(10.0f)
        );

    SECTION("Shared options are given before the command")
    {
        auto const arguments = tests::parse(cli, {"--root-path=C://Users/foo/Desktop/", "open-window", "-w=800", "-h=600"});

        REQUIRE(arguments.has_value());
        REQUIRE(arguments->shared_arguments.root_path == "C://Users/foo/Desktop/");
        REQUIRE(arguments->shared_arguments.dry_run == false);
        REQUIRE(arguments->command.index() == 0);
    }
    SECTION("Fail to parse because of unknown argument")
    {
        auto const arguments = tests::parse(cli, {"--undefined=Hello", "open-window", "-w=800", "-h=600"});
        
        REQUIRE(!arguments.has_value());
    }
    SECTION("Fail to parse because of shared argument after command")
    {
        auto const arguments = tests::parse(cli, {"open-window", "--root-path=C://Users/foo/Desktop/", "-w=800", "-h=600"});

        REQUIRE(!arguments.has_value());
    }
    SECTION("No values given for shared arguments. Default values are used and command is directly parsed")
    {
        auto const arguments = tests::parse(cli, {"open-window", "-w=800", "-h=600"});

        REQUIRE(arguments.has_value());
        REQUIRE(arguments->shared_arguments.root_path == ".");
        REQUIRE(arguments->shared_arguments.dry_run == false);
        REQUIRE(arguments->command.index() == 0);
    }
}

TEST_CASE("Commands with implicit command")
{
    constexpr auto cli = tests::Help()
        | clp_Opt(int, width)["-w"]["--width"]
            ("Width of the screen in pixels.")
            .default_to(1920)
        | clp_Opt(int, height)["-h"]["--height"]
            ("Height of the screen in pixels.")
            .default_to(1080)
        | clp_Opt(bool, fullscreen)["--fullscreen"]
            ("Whether to start the application in fullscreen or not.")
            .default_to(false)
            .implicitly(true)
        | clp_Opt(std::string, starting_level) ["--starting-level"]
            ("Level to open in the editor.")
            .hint("level-name");

    SECTION("Help matched")
    {
        auto const options = tests::parse(cli, {"--help"});

        REQUIRE(options.has_value());
        REQUIRE(options->index() == 0);
    }
    SECTION("Implicit command matched")
    {
        auto const options = tests::parse(cli, {"-w=50", "-h=40", "--starting-level=foo"});

        using Args = clp_command_type(cli, 1);

        REQUIRE(options.has_value());
        std::visit(overload(
            [](tests::ShowHelp) { REQUIRE(false); },
            [](Args args) 
            {
               REQUIRE(args.width == 50);
               REQUIRE(args.height == 40);
               REQUIRE(args.fullscreen == false);
               REQUIRE(args.starting_level == "foo");
            }
        ),  *options);
    }
}

TEST_CASE("CommandSelector::to_string")
{
    constexpr auto cli =
        clp::Command("open-window", "Open a test window.",
            clp_Opt(int, width)["-w"]["--width"]("Width of the screen") |
            clp_Opt(int, height)["-h"]["--height"]("Height of the screen")
        )
        | clp::Command("fetch-url", "Fetch the given url and print the HTTP response.",
            clp_Opt(std::string, url)["--url"]("Url to fetch") |
            clp_Opt(int, max_attempts)["--max-attempts"]("Maximum number of attempts before failing") |
            clp_Opt(float, timeout)["--timeout"]("Time to wait for response before failing the attempt").default_to(10.0f)
        )
        | tests::Help();

    std::string const help_text = cli.to_string();

    constexpr auto expected =
        "open-window              Open a test window.\n"
        "fetch-url                Fetch the given url and print the HTTP response.\n"
        "help, --help, -h, -?     Show help about the program or a specific command.\n"
        ;

    REQUIRE(help_text == expected);
}

TEST_CASE("CommandsWithSharedOptions::to_string")
{
    constexpr auto cli =
        clp::SharedOptions(
            clp_Opt(std::string, root_path)["--root-path"]("Root directory of the project.")
                .default_to("."sv)
                .hint("path")
            | clp_Flag(dry_run) ["--dry-run"]("Print the actions that the command would perform without making any change.")
        ) 
        | clp::Command("open-window", "Open a test window.",
            clp_Opt(int, width)["-w"]["--width"]("Width of the screen") |
            clp_Opt(int, height)["-h"]["--height"]("Height of the screen")
        )
        | clp::Command("fetch-url", "Fetch the given url and print the HTTP response.",
            clp_Opt(std::string, url)["--url"]("Url to fetch") |
            clp_Opt(int, max_attempts)["--max-attempts"]("Maximum number of attempts before failing") |
            clp_Opt(float, timeout)["--timeout"]("Time to wait for response before failing the attempt").default_to(10.0f)
        );

    std::string const help_text = cli.to_string();

    constexpr auto expected =
        "Shared options:\n"
        "  --root-path <path>                    Root directory of the project.\n"
        "                                        By default: .\n"
        "  --dry-run <bool>                      Print the actions that the command would perform without making any change.\n"
        "                                        By default: false\n"
        "                                        Implicitly: true\n"
        "\n"
        "Commands:\n"
        "  open-window            Open a test window.\n"
        "  fetch-url              Fetch the given url and print the HTTP response.\n"
        ;

    REQUIRE(help_text == expected);
}

TEST_CASE("CommandsWithImplicitCommand")
{
    constexpr auto cli = tests::Help()
        | clp_Opt(int, width)["-w"]["--width"]
            ("Width of the screen in pixels.")
            .default_to(1920)
        | clp_Opt(int, height)["-h"]["--height"]
            ("Height of the screen in pixels.")
            .default_to(1080)
        | clp_Opt(bool, fullscreen)["--fullscreen"]
            ("Whether to start the application in fullscreen or not.")
            .default_to(false)
            .implicitly(true)
        | clp_Opt(std::string, starting_level) ["--starting-level"]
            ("Level to open in the editor.")
            .hint("level-name");

    std::string const help_text = cli.to_string();

    constexpr auto expected =
        "Commands:\n"
        "  help, --help, -h, -?   Show help about the program or a specific command.\n"
        "\n"
        "Options:\n"
        "  -w, --width <int>                     Width of the screen in pixels.\n"
        "                                        By default: 1920\n"
        "  -h, --height <int>                    Height of the screen in pixels.\n"
        "                                        By default: 1080\n"
        "  --fullscreen <bool>                   Whether to start the application in fullscreen or not.\n"
        "                                        By default: false\n"
        "                                        Implicitly: true\n"
        "  --starting-level <level-name>         Level to open in the editor.\n"
        ;

    REQUIRE(help_text == expected);
}

TEST_CASE("Single positional argument cli")
{
    constexpr auto cli = clp_Arg(int, width, "width")("Width of the screen in pixels");

    SECTION("Found")
    {
        auto const options = tests::parse(cli, {"1920"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 1920);
    }
    SECTION("Not found")
    {
        auto const options = tests::parse(cli, {});

        REQUIRE(!options.has_value());
    }
    SECTION("Fail to parse")
    {
        auto const options = tests::parse(cli, {"foo"}); // Cannot parse an int from "foo"

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("Positional arguments must be provided in order")
{
    constexpr auto cli =
        clp_Arg(int, width, "width")("Width of the screen in pixels.")
        | clp_Arg(std::string_view, username, "username")("Username to login with.");

    SECTION("In order")
    {
        auto const options = tests::parse(cli, {"1920", "Foobar"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 1920);
        REQUIRE(options->username == "Foobar");
    }
    SECTION("Out of order")
    {
        auto const options = tests::parse(cli, {"Foobar", "1920"});

        REQUIRE(!options.has_value());
    }
    SECTION("One missing")
    {
        auto const options = tests::parse(cli, {"1920"});

        REQUIRE(!options.has_value());
    }
}

TEST_CASE("A positional argument and a flag")
{
    constexpr auto cli =
        clp_Arg(int, width, "width")("Width of the screen in pixels.")
        | clp_Flag(fullscreen)["--fullscreen"]("Whether to start the application in fullscreen or not.");

    SECTION("Only argument")
    {
        auto const options = tests::parse(cli, {"1920"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 1920);
        REQUIRE(options->fullscreen == false);
    }
    SECTION("Both")
    {
        auto const options = tests::parse(cli, {"1920", "--fullscreen"});

        REQUIRE(options.has_value());
        REQUIRE(options->width == 1920);
        REQUIRE(options->fullscreen == true);
    }
    SECTION("Argument missing")
    {
        auto const options = tests::parse(cli, {"--fullscreen"});

        REQUIRE(!options.has_value());
    }
    SECTION("Flag first")
    {
        auto const options = tests::parse(cli, {"--fullscreen", "1920"});

        REQUIRE(!options.has_value());
    }
}

// Tasks
// - Refactor parse to make it return either a parse result or an error
//     - Decide whether to use variant or expected. (Maybe expected from aeh?)
//     - Decide whether error is a structure (convertible to string) or a string directly.
//     - Maybe go with string first for simplicity and decide whether to change it to a structure in the future.
