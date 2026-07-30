# Assemblia®

Formerly a project made in a team for university. The task was:

- To make a game
- ...that didn't use an existing game enging
- ...in C++
- ...with dungeon crawl / rogue-like elements

I (Luna Fox) decided to write a small engine called miniflow in order to combine this assignment with learning how to program Vulkan. I was not very good at it. Now I am just re-using the code to experiment with Vulkan 4 years later. Don't expect much!

This game is scriptable via Lua (LuaJIT 2.1). Engine is written in C, Game is written in C++. It tried to use data-oriented design. I uh, misunderstood what it was.

Look, this project was written years ago when I was young, idealistic, and somewhat mentally unstable. I had opinions that I can  best describe as "shitty takes" back then. Don't judge the young me too harshly.

# Building

The game is currently targeted at 64-bit x86 architecture. ARM64 is supported. This is a Linux-first system, but is extensively tested on both Linux and Windows. MacOS support is available too, but not as extensively tested.

**Supported Platforms:** Linux x86_64 (X11, via `-platform x11`, or by default if Wayland isn't available), Linux x86_64 (Wayland), Windows x86_64, macOS arm64

## Prerequisites

**In general (to play the game):**

- GPU which has a Vulkan-compatible driver, which supports at least Vulkan 1.1
- At least 384MB of VRAM (512MB or more of total VRAM recommended)
- At least 512MB of free RAM (4GB or more of system RAM recommended)
- For best results, CPU with at least 4 logical cores (threads)
- 16MB of free disk space for save files (may increase if you make a ton of saves)
- Whatever this repository takes on the hard drive for game files

**On Linux:**

- gcc16 or newer (must support C++20 and C11 standards)
- git
- cmake
- Vulkan-compatible GPU driver which provides `libvulkan.so.1` and a Vulkan implementation
- Headers and libraries for X11 *and* Wayland
- glslang development package with CMake support (e.g. `glslang-dev`/`glslang-devel`, or the Vulkan SDK), used to compile shaders at startup

**On Windows:**

- Windows 11
- Visual Studio 2022 newer with C++ development features selected (which must provide MSVC compiler with vc140 / MSVC 2015)
- cmake commandline tools
- git commandline tools
- Vulkan-compatible GPU driver which provides `vulkan-1.dll` and a Vulkan implementation
- Visual C++ Redistributables 2015
- The [Vulkan SDK](https://vulkan.lunarg.com/) (provides glslang, used to compile shaders at startup)

## Compiling

1. Pull all submodules (`git submodule update --init --recursive`)
2. On Linux, `make`. On Windows, run `build.bat`

## Compiling a release version

**Linux:**

`make dist` should create a dist/ folder, build the project and copy all of the assets to it. The folder should be ready for distribution.

**Windows:**

`build_release.bat` should create a dist/ folder, build the project and copy all of the assets to it. The folder should be ready for distribution.

## Recompiling

On Linux, run `make clean`. On Windows, delete `build/` and `dist/` folders.

## Shaders

Shader sources live in `shaders/` and are compiled to SPIR-V at startup using [glslang](https://github.com/KhronosGroup/glslang). Compiled shaders are cached in a `shader_cache/` directory (created next to the working directory, not tracked by git) and are only recompiled when the source file on disk is newer than its cached SPIR-V. To iterate on a shader, just edit it and restart the game.

This requires the glslang development package at build time (see prerequisites).

## Linting

Linting is only officially supported on Linux. However, on Windows you could get away with just running `clang-format -i *.cpp` within the relevant directory. To lint, LLVM tools must be installed, providing the `clang-format` commandline tool.

`make lint` will format all source files of the project to be compliant with our coding standard.

## See also

* [The Wiki Pages](https://github.com/infomediadesign/projektarbeit-ii-team-7/wiki)
* [Basic Git Guide](https://github.com/infomediadesign/projektarbeit-ii-team-7/wiki/Git-Basics)
* [C++ Tutorial](https://learncpp.com)
* [LLVM Tools, such as clang-format](https://clang.llvm.org/)
* [The Spec](https://github.com/infomediadesign/projektarbeit-ii-team-7/wiki/Game-Spec)
