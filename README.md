# Command line parser

This is a toy command line parser meant to be a proof of concept and an exercise on template metaprogramming. It aims to give a command line interface where a command line parser function is declared. This is a function that maps (argc, argv) to an anonymous structure declared by the library based on the parser function declaration that contains the variables read from the command line. It supports optional arguments with default values, conditions to check if an option has a correct value, customization points for parsing user defined types, and declaring subparsers for commands like git does (git commit, git push...).

Many features present in common command line parsing libraries are most likely missing because this is more a proof of concept than a production quality library. Please do not use this in production. More features may or may not be added in the future depending on whether I feel like it.

The library currently (as of 20-11-2020) only builds on latest Visual Studio 2019 (Version 16.8). GCC and clang both reject the code. Clang because lambdas in unevaluated contexts have not been implemented yet. GCC because it doesn't allow declaring new types inside a decltype expression, which is necessary for the interface of the library.

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
    | clp_Opt(int, number_of_threads)["--num-threads"]
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
