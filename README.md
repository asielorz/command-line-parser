# Command line parser

This is a toy command line parser meant to be a proof of concept and an exercise on template metaprogramming. It aims to give a command line interface where a command line parser function is declared. This is a function that maps (argc, argv) to an anonymous structure declared by the library based on the parser function declaration that contains the variables read from the command line. It supports optional arguments with default values, conditions to check if an option has a correct value, customization points for parsing user defined types, and declaring subparsers for commands like git does (git commit, git push...).

Many features present in common command line parsing libraries are most likely missing because this is more a proof of concept than a production quality library. Please do not use this in production. More features may or may not be added in the future depending on whether I feel like it.

The library currently (as of 20-11-2020) only builds on latest Visual Studio 2019 (Version 16.8). GCC and clang both reject the code. Clang because lambdas in unevaluated contexts have not been implemented yet. GCC because it doesn't allow declaring new types inside a decltype expression, which is necessary for the interface of the library.

This sample code shows how to declare a command line interface with several commands and subparsers for each command.

```cpp
constexpr auto cli =
    Command("open-editor",
        Opt(int, width)["-w"]["--width"]
            ("Width of the screen in pixels.")
            .default_to(1920)
            .check([](int width){ return width > 0 && width <= 3840; }, "Width must be between 0 and 3840 (4k).")
        | Opt(int, height)["-h"]["--height"]
            ("Height of the screen in pixels.")
            .default_to(1080)
            .check([](int height) { return height > 0 && height <= 2160; }, "Height must be between 0 and 2160 (4k).")
        | Opt(bool, fullscreen)["-fullscreen"]
            ("Whether to start the application in fullscreen or not.")
            .default_to(false)
        | Opt(std::string, starting_level) ["--starting-level"]
            ("Level to open in the editor.")
            .check(is_level_name, "Level does not exist.")
    )
    & Command("bake-data",
        Opt(std::string, level)["--level"]
            ("Level to bake data for.")
            .check(is_level_name, "Level does not exist.")
        | Opt(Platform, platform)["--platform"]
            ("Platform for which to bake the data.")
        | Opt(GraphicsAPI, graphics_api)["--graphics-api"]
            ("Graphics API for which to bake the data.")
        | Opt(int, number_of_threads)["--num-threads"]
            ("Number of threads to use in data generation.")
            .default_to(8)
            .check([](int number_of_threads) { return number_of_threads > 0 && number_of_threads <= std::thread::hardware_concurrency(); }, "Invalid number of threads.")
    );

auto const arguments = cli.parse(argc, argv);

if (arguments)
{
    switch (arguments->index())
    {
        case 0:
        {
            auto const editor_args = std::get<0>(*arguments);
            open_editor(editor_args.width, editor_args.height, editor_args.fullscreen, editor_args.starting_level);
            break;
        }
        case 1:
        {
            auto const bake_args = std::get<1>(*arguments);
            bake_data(bake_args.level, bake_args.platform, bake_args.graphics_api, bake_args.number_of_threads);
        }
    }
}
```
