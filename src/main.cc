#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"

#include "clp.hh"
#include <typeinfo>

namespace tests
{
    template <clp::Parser P>
    constexpr auto parse(P parser, std::initializer_list<char const *> args) noexcept
    {
        int const argc = int(args.size());
        auto const argv = std::data(args);
        return clp::parse(parser, argc, argv);
    }
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
        auto const options = tests::parse(cli, {"-f=50"});

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
        Command("open-window", 
            clp_Opt(int, width)["-w"]["--width"]("Width of the screen") |
            clp_Opt(int, height)["-h"]["--height"]("Height of the screen")
        )
        | Command("fetch-url", 
            clp_Opt(std::string, url)["--url"]("Url to fetch") |
            clp_Opt(int, max_attempts)["--max-attempts"]("Maximum number of attempts before failing") |
            clp_Opt(float, timeout)["--timeout"]("Time to wait for response before failing the attempt").default_to(10)
        );
        
    int argc = 0;
    char const * const * argv = nullptr;
    clp::parse(cli, argc, argv);

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
    auto const cli = clp_Opt(bool, some_flag)["--flag"]
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

    std::string const str = clp::to_string(cli);

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
        clp::Command("open-window", 
            clp_Opt(int, width)["-w"]["--width"]("Width of the screen") |
            clp_Opt(int, height)["-h"]["--height"]("Height of the screen")
        )
        | clp::Command("fetch-url",
            clp_Opt(std::string, url)["--url"]("Url to fetch") |
            clp_Opt(int, max_attempts)["--max-attempts"]("Maximum number of attempts before failing") |
            clp_Opt(float, timeout)["--timeout"]("Time to wait for response before failing the attempt").default_to(10)
        )
        | clp::Help();

    constexpr auto visitor = overload(
        [](clp::ShowHelp) { return true; },
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
