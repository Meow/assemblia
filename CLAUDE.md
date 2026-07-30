# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**Assemblia** (internal codename **miniflow**) is a Vulkan-based dungeon-crawler with a Dragon Quest–style battle system. Custom engine, no game framework. The final binary is `miniflow`.

## Building & running

Submodules must be pulled first: `git submodule update --init --recursive`.

| Task | Linux / macOS | Windows |
|------|---------------|---------|
| Debug build | `make` (= `make configure build`) | `build.bat` |
| Build + run | `make run` | run `build.bat` then the exe |
| Run existing build | `make exec` | `./build/engine/miniflow` |
| Release / distributable | `make dist` | `build_release.bat` |
| Clean | `make clean` | delete `build/` and `dist/` |

CMake targets Debug by default. The build is driven by `CMakeLists.txt` → `src/engine/CMakeLists.txt` (which pulls in `render`, `state`, `input`, `logic`, and `../game`).

### macOS (arm64, via MoltenVK)

macOS is supported through **MoltenVK** (Vulkan-over-Metal). Prerequisites (Homebrew): `brew install vulkan-loader vulkan-headers molten-vk glfw glslang` — `vulkan-loader` + `molten-vk` + `glslang` are needed at runtime; the loader auto-discovers the MoltenVK ICD, and CMake bakes the loader's absolute path into GLFW so no `VULKAN_SDK`/`DYLD_LIBRARY_PATH` is required. Then `make run` as usual.

Platform-specific porting notes (things to preserve when touching engine internals):
- **Cocoa is single-threaded for UI.** GLFW window creation and `glfwPollEvents` must run on the main thread, so on macOS `main.c` runs `render_perform` on the main thread (rather than spawning it) and events are polled in the render loop — mirroring the Windows path. `input.c` therefore skips `glfwPollEvents` on macOS.
- **Portability extensions** are enabled in `geyser.c`: the instance opts into `VK_KHR_portability_enumeration` (+ the enumerate-portability flag) and the device enables `VK_KHR_portability_subset` when advertised. Both are guarded so Linux/Windows are unaffected.
- The vendored `glad` Vulkan header predates these extensions, so their name/flag constants are `#define`d defensively at the top of `geyser.c`.

### Shaders

Shader sources live in `shaders/*.{vert,frag}` (top-level, shipped with the game like `lua/` and `assets/`). They are **compiled to SPIR-V at startup** by `src/engine/render/shader_compiler.c` using the glslang C API, and cached in `shader_cache/` (gitignored, created next to the working directory). A shader is recompiled only when its source mtime is newer than the cached `.spv`; otherwise the cache is loaded directly. Editing a `.vert`/`.frag` takes effect on the next launch — no build step. A compile error prints the glslang log and exits before the window opens. glslang is a build/runtime dependency (Homebrew `glslang` on macOS, `glslang-dev` or the Vulkan SDK elsewhere), found via `find_package(glslang CONFIG)` in `src/engine/render/CMakeLists.txt`.

### Lint

`make lint` runs `clang-format -i` over all of `src/` per `.clang-format`. Run it before committing. There is currently **no test suite** (the `test` phony target has no body).

## Architecture

### Two-language split, bridged by one header

- **Engine** = C11, under `src/engine/`. Owns the window, Vulkan, threads, input, timing, and the shared `GameState`.
- **Game** = C++20, under `src/game/`, compiled to `libgame.a`. Owns gameplay: entities, controllers, levels, Lua.
- The **only** contract between them is [`src/game/interface.h`](src/game/interface.h) — a set of `extern "C"` event callbacks (`game_initialize`, `game_tick`, `game_lazy_tick`, `game_paused_tick`, `game_adjust_renderables`, `game_create_bindings`). The engine calls these; the C++ side dispatches into the `Game` class. When adding a new engine→game event, it must be declared here.

### Threading model (read before touching state)

[`src/engine/main.c`](src/engine/main.c) spawns **three threads** that all share one `GameState*` guarded by a single `mutex_t`:

- **logic** ([`logic.c`](src/engine/logic/logic.c)) — runs `game_tick` at `state->tickrate` Hz; calls `game_lazy_tick` every 16th tick; calls `game_paused_tick` (and skips normal ticks) while `GS_PAUSED` is set.
- **render** ([`render.c`](src/engine/render/render.c)) — window + draw loop; calls `game_adjust_renderables`.
- **input** ([`input.c`](src/engine/input/input.c)) — polls GLFW. **Note: on Windows, input is polled on the render thread, not its own thread.**

`GameState` (in [`state/state.h`](src/engine/state/state.h)) is a flat struct; boolean state is packed into `flags` via the `GameFlag` enum (`GS_PAUSED`, `GS_DEBUG`, `GS_EXIT`, `GS_FULLSCREEN`, `GS_VERBOSE`) and accessed through `game_add_flag`/`game_remove_flag`/`game_is_*` helpers. Any cross-thread mutation of game data must hold the `lock`.

### Rendering: "Geyser"

[`geyser.c`/`geyser.h`](src/engine/render/geyser.h) is a bespoke minimal Vulkan middleware layer used only by this project. Renderer draws a flat list of `Renderable`s plus `GlyphText` objects; the game populates/sorts them each frame in `game_adjust_renderables` (see `Game::update_renderables` and `Game::compare_renderables`).

### Game structure (C++)

- [`Game`](src/game/game.h) is the central object. It holds an `EntityManager`, the four controllers, the current `Level`, and the `lua_State`. A `GameStage` enum (`GS_MENU`, `GS_OVERWORLD`, `GS_DUNGEON`, `GS_BATTLE`) selects which controller is active.
- **Controllers** ([`src/game/controllers/`](src/game/controllers/)) — one per stage (menu/overworld/dungeon/battle). All share the `CONTROLLER_METHOD_DEFINITIONS` macro shape (`init`/`update`/`update_lazy`/`update_paused`/`update_renderables`/`process_input`/`destroy`). Adding a stage means adding a controller and wiring it in `Game`.
- **Entities** — `EntityManager` uses a **fixed-size array** `entities[MAX_ENTITIES]` (no dynamic allocation of entities); `find_free_ent()` recycles slots. `Player` is a special entity.
- **Levels** — Tiled maps. `.tmx` sources in `levels/` are exported to JSON and parsed with **simdjson** (vendored, compiled into `libgame`). See [`level.cpp`](src/game/level.cpp).

### Lua scripting (LuaJIT)

Gameplay is scripted in `lua/`, loaded at runtime (copied into `dist/lua` for release).

- C++ exposes native functions to Lua as `Game::lua_*` statics and the `lua_*` free functions in [`src/game/lua/`](src/game/lua/) (e.g. `common.cpp`, `entity.cpp`, `text.cpp`). To expose new engine capability to scripts, add a `lua_*` C function and register it.
- [`lua/init.lua`](lua/init.lua) bootstraps everything. It defines a custom **`include`** (not `require`) that resolves paths relative to the current file — use `include 'foo'`, not `require`, for project files. Load order: `table_utils`, `math_utils`, `enums`, `event`, `entity`, `game/game`.
- `lua/game/` holds the actual gameplay logic (`game.lua`, `player.lua`, `battle.lua`, `tileset_ent.lua`). Input binds are set up in Lua via `game.bind(...)` (see `GAME:create_bindings`), mapping key+action bitmasks to integer command IDs.
- **F5 hot-reloads Lua** at runtime (`REFRESH_CMD`); `LUA_STARTED` guards re-init.

### Numeric types

The codebase uses short type aliases everywhere: `i8/i16/i32/i64`, `u8/…/u64`, `f32/f64` (from [`src/engine/types/numeric.h`](src/engine/types/numeric.h)). Math primitives (`Vector*`, `Matrix*`, `Quaternion`) live in `src/engine/types/`. Follow this convention rather than raw C types.

### font_rasterizer

[`font_rasterizer/`](font_rasterizer/) is a **separate Rust tool** (its own Cargo project) that rasterizes the bitmap font used by the glyph renderer. It is not part of the CMake build.

## Vendored dependencies

All in `vendor/` as git submodules (except `glad`/`simdjson` which are checked in): GLFW (windowing), Vulkan-Headers, LuaJIT (+ luajit-cmake), stb, simdjson. Don't edit vendored code.
