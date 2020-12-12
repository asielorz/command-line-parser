# The dodo command line parsing library

Dodo is a fully featured C++20 command line parsing library, with support for positional arguments, named options and git style commands. The library allows for declaratively expressing a command line parser function. This is a function that maps (argc, argv) to an anonymous structure declared by the library based on the parser function declaration that contains the variables read from the command line. It supports optional arguments with default values, conditions to check if an option has a correct value, customization points for parsing user defined types, and declaring subparsers for commands like git does (git commit, git push...). The name just follows the trend of giving animal names to software projects.

## Compliance

The library in implemented in standard C++20 and currently (as of 12-12-2020) only builds on latest Visual Studio 2019 (Version 16.8). GCC and clang both reject the code. Clang because lambdas in unevaluated contexts have not been implemented yet. GCC because it doesn't allow declaring new types inside a decltype expression, which is necessary for the interface of the library.

## License

Dodo is distributed under the MIT software licence. (See accompanying file LICENSE.txt)

## Features

- Header only with no external dependencies (except the std library).

- Define your interface once to get parsing, type conversions and usage strings with no redundancy.

- Composing. Each option or argument is an independent parser. Combine these to produce a compound parser.

- Define structures that will hold the parsed arguments implicitly from the parser.

- Typed arguments. Customization of conversion to and from string through traits and custom parsers.

- `dodo::expected<value, error>` type for error propagation.

- Models POSIX standards for short and long opt behavior.

- Validation functions for arguments that limit the values they can take.

- Automatic generation of --help text from the parser function.

- Command parsers, defined as a variant of the arguments taken by the parser of each of the commands.

## Using

Dodo is a single header library. To use the library, just `#include "dodo.hh"`

A parser for a single option can be created like this:

```cpp
constexpr auto cli = 
	dodo_Opt(int, width)
		["-w"]["--width"]
		("Width of the window in pixels.");
```

The parser can be used directly like this:

```cpp
auto const result = cli.parse(argc, argv);
if (result)
	do_stuff_with(result->width);
else
	std::cerr << result.error() << '\n';
```

Parsers can be combined with `operator |`, like this:

```cpp
constexpr auto cli
	= dodo_Opt(int, width)
		["-w"]["--width"]
		("Width of the window in pixels.")
	| dodo_Opt(int, height)
		["-h"]["--height"]
		("Height of the window in pixels.")
	| dodo_Opt(bool, fullscreen)
		["--fullscreen"]
		("Start the application in fullscreen mode.")
	| dodo_Opt(std::string, username)
		["-n"]["--name"]
		("Username used to log in to the server.");
```

In this case the structure returned by parse will contain the members `width`, `height`, `fullscreen` and `username`.

```cpp
auto const args = cli.parse(argc, argv);
if (!result)
{
	std::cerr << result.error() << '\n';
	exit(1);
}
auto const window_handle = open_window(args->width, args->height, args->fullscreen);
auto const connection_handle = start_session(server_url, args->username);
```

`dodo_Opt` specifies named options that start with `-`. So the program above could be invoked with the command line `-w=1920 -h=1080 -n=foobar --fullscreen=true` for example. The order of the arguments is not important since they are found by name. The user must input the arguments as `--name=value`. Inputing arguments separated by a space as `--name value` is *not* supported.

For positional arguments `dodo_Arg` is used. With `dodo_Arg`, the user does not need to type the name of the option. However, positional arguments must be given in order and before named options. For example:

```cpp
constexpr auto cli
	= dodo_Arg(int, width, "width")
		("Width of the window in pixels.")
	| dodo_Arg(int, height, "height")
		("Height of the window in pixels.")
	| dodo_Arg(std::string, username, "username")
		["-n"]["--name"]
		("Username used to log in to the server.");
```

The parser above would succesfully parse the command line `1920 1980 foobar`, however the line `1920 foobar 1080` would fail because the program expects to find the height in second position, and there is no way of converting `"foobar"` to an integer.

### Default value

By default, if a named argument is not provided by the user, the function will fail to parse. However, it is possible to provide a default value to an option. If one is provided and the user does not input an option, parsing will succeed and the option will take the default value. A default value is added to an option through the `by_default` method.

constexpr auto cli
	= dodo_Opt(int, width)
		["-w"]["--width"]
		("Width of the window in pixels.")
		.by_default(1920)
	| dodo_Opt(int, height)
		["-h"]["--height"]
		("Height of the window in pixels.")
		.by_default(1080)
	| dodo_Opt(bool, fullscreen)
		["--fullscreen"]
		("Start the application in fullscreen mode.")
		.by_default(false)
	| dodo_Opt(std::string, username)
		["-n"]["--name"]
		("Username used to log in to the server.")
		.by_default("anonymous"sv);
```

It is important to notice that the default value does not need to be of the exact same type as the option as long as it is convertible. This allows for having constexpr parsers for string or vector types, by using types like string_view for the default value.

### Implicit value and flags




























This sample code shows how to declare a command line interface with several commands and subparsers for each command.

```cpp
//************************************************************************************************************
// open_editor.hh
constexpr auto open_editor_cli = clp_Opt(int, width)["-w"]["--width"]
        ("Width of the screen in pixels.")
        .default_to(1920)
        .check([](int width){ return width > 0 && width <= 3840; }, "Width must be between 0 and 3840 (4k).")
    | clp_Opt(int, height)["-h"]["--height"]
        ("Height of the screen in pixels.")
        .default_to(1080)
        .check([](int height) { return height > 0 && height <= 2160; }, "Height must be between 0 and 2160 (4k).")
    | clp_Flag(fullscreen)["--fullscreen"]
        ("Whether to start the application in fullscreen or not.")
    | clp_Opt(std::string, starting_level) ["--starting-level"]
        ("Level to open in the editor.")
        .hint("level-name")
        .check(is_level_name, "Level does not exist.");

using OpenEditorArgs = clp_parse_result_type(open_editor_cli);
int open_editor_main(OpenEditorArgs const & args);

//************************************************************************************************************
// bake_data.hh
constexpr auto bake_data_cli = clp_Opt(std::string, level)["--level"]
        ("Level to bake data for.")
        .check(is_level_name, "Level does not exist.")
    | clp_Opt(Platform, platform)["--platform"]
        ("Platform for which to bake the data.")
    | clp_Opt(GraphicsAPI, graphics_api)["--graphics-api"]
        ("Graphics API for which to bake the data.")
    | clp_Opt(unsigned, number_of_threads)["--num-threads"]
        ("Number of threads to use in data generation.")
        .default_to(8)
        .check([](unsigned number_of_threads) { return number_of_threads > 0 && number_of_threads <= std::thread::hardware_concurrency(); }, "Invalid number of threads.");

using BakeDataArgs = clp_parse_result_type(bake_data_cli);
int bake_data_main(BakeDataArgs const & args);

//************************************************************************************************************
// main.cc
int main(int argc, char const * const argv[])
{
    // Define command line interface as a composition of the interface of each system.
    constexpr auto cli =
        clp::Command("open-editor", open_editor_cli)
        | clp::Command("bake-data", bake_data_cli);

    auto const arguments = cli.parse(argc, argv);

    if (arguments)
    {
        return std::visit(overload(
            [](OpenEditorArgs const & args) { return open_editor_main(args); },
            [](BakeDataArgs const & args) { return bake_data_main(args); }
        ), *arguments);
    }
    else
    {
        return -1;
    }
}
```
