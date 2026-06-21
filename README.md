# Arcade Blocks II

Native C++20 version of Arcade Blocks II: an SDL3 brick-breaker with a menu
flow, settings, localization, sprite/audio assets, JSON levels, Box2D-backed
gameplay physics, and automated smoke tests.

The repository also keeps `lbreakouthd-1.2.2/` as imported reference material
and level/theme source data. The active game implementation is in `src/`,
`assets/`, `tests/`, `tools/`, and the top-level CMake files.

## Status

The project is a playable native foundation rather than a finished release.
Implemented areas include:

- SDL3 application runtime with windowing, renderer, input, fixed timestep,
  logging, and platform-specific data paths.
- Main menu, level selection flow, pause screen, settings, help, HUD, loading,
  level-complete, and debug UI views.
- JSON settings with validation, reset support, isolated test settings files,
  and English/Russian localization data.
- Texture, sprite atlas, font, music, and sound loading from the `assets/`
  tree.
- Level loading for the native Arcade Blocks level sets and converted classic
  level data.
- Box2D-based physics layer and gameplay model for paddle, ball, bricks,
  lives, score, boss levels, and smoke-test scenarios.
- Catch2 unit tests plus CTest application smoke tests.

Some directories are placeholders for planned systems (`src/cutscenes`,
`src/editor`, `src/multiplayer`, `src/persistence`) and are not part of the
current CMake target yet.

## Requirements

Common requirements:

- CMake 3.25 or newer
- Ninja
- C++20 compiler
- Git, unless all third-party dependencies are already installed as CMake
  packages

Default builds use CMake `FetchContent` for pinned dependencies, so the first
configure needs network access.

Pinned dependencies in `CMakeLists.txt`:

| Dependency | Version |
| --- | --- |
| SDL3 | 3.4.10 |
| SDL3_image | 3.4.4 |
| SDL3_mixer | 3.2.4 |
| SDL3_ttf | 3.2.2 |
| Box2D | 3.1.1 |
| Dear ImGui | 1.92.4 |
| nlohmann/json | 3.12.0 |
| Catch2 | 3.9.1 |

On Ubuntu/Debian, install the usual native build and SDL platform packages:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  libasound2-dev \
  libdbus-1-dev \
  libegl1-mesa-dev \
  libgl1-mesa-dev \
  libudev-dev \
  libwayland-dev \
  libx11-dev \
  libxcursor-dev \
  libxext-dev \
  libxfixes-dev \
  libxi-dev \
  libxinerama-dev \
  libxkbcommon-dev \
  libxrandr-dev \
  wayland-protocols
```

On Windows, use Visual Studio 2022 with the "Desktop development with C++"
workload and make sure `cmake`, `ninja`, and the MSVC toolchain are available
from the active shell.

## Build

Configure, build, and test the debug preset:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Release build:

```bash
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

Linux sanitizer build:

```bash
cmake --preset linux-asan-ubsan
cmake --build --preset linux-asan-ubsan
ctest --preset linux-asan-ubsan
```

Windows presets:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Preset build directories are created under `build/<preset-name>/`. For example,
the Linux debug executable is:

```bash
./build/linux-debug/ArcadeBlocksII
```

To use installed dependency packages instead of `FetchContent`, configure with:

```bash
cmake --preset linux-debug -DARCADEBLOCKS_USE_SYSTEM_DEPS=ON
```

## Run

From the repository root:

```bash
./build/linux-debug/ArcadeBlocksII
./build/linux-debug/ArcadeBlocksII --level 1 --windowed
./build/linux-debug/ArcadeBlocksII --level 10 --debug
./build/linux-debug/ArcadeBlocksII --assets-dir ./assets --no-audio
```

Useful command-line options:

| Option | Purpose |
| --- | --- |
| `--help`, `-h` | Print supported options. |
| `--version` | Print the application version. |
| `--assets-dir <path>` | Override automatic asset discovery. |
| `--settings-file <path>` | Use an isolated settings file. |
| `--level <number>` | Start directly in a level. |
| `--windowed` / `--fullscreen` | Override configured window mode. |
| `--reset-settings` | Rewrite settings with defaults before launch. |
| `--no-audio` | Disable audio initialization. |
| `--debug` | Enable verbose logging and debug defaults. |
| `--perf-summary` | Emit one machine-readable performance summary on exit. |
| `--ui-scale <0.75..2.0>` | Override UI scale. |
| `--smoke-frames <n>` | Run for `n` frames and exit. |
| `--smoke-scenario <name>` | Open a deterministic smoke-test route. |

Smoke scenarios are `main-menu`, `settings`, `settings-save`,
`settings-cycle`, `help`, `help-cycle`, `pause`, `pause-settings`, and
`pause-help`.

Headless smoke run:

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./build/linux-release/ArcadeBlocksII \
  --assets-dir ./assets \
  --windowed \
  --no-audio \
  --smoke-frames 60 \
  --smoke-scenario help-cycle \
  --perf-summary
```

## Controls

Default controls:

- `Up` / `Down`: move menu focus.
- `Enter` / `Space`: activate focused menu action.
- `Esc`: back, pause, or close the current overlay.
- `Left` / `Right` or `A` / `D`: move paddle.
- Mouse movement: move paddle while gameplay has focus.
- `Space` or left mouse button: launch ball.
- `F1`: debug overlay.
- `F2`: physics debug draw.
- `F3`: asset/audio stats overlay and log snapshot.
- `F4`: restart the current level.
- `F11` or `Alt+Enter`: toggle fullscreen.
- `Q`: quit during development runs.

Settings can change language, audio, video, controls, gameplay values, and key
bindings. The settings store validates duplicate bindings, reserved debug keys,
empty player names, and numeric ranges.

## Data Paths

Assets are discovered automatically by looking for an `assets/` directory near
the current working directory or executable. A valid asset directory must
contain `sprites/`, `sounds/`, `music/`, and `levels/`.

Logs and settings are written to the platform data directory:

| Platform | Data directory |
| --- | --- |
| Linux | `$XDG_DATA_HOME/arcadeblocks2` or `~/.local/share/arcadeblocks2` |
| Windows | `%APPDATA%/ArcadeBlocksII` |
| macOS | `~/Library/Application Support/ArcadeBlocksII` |

The main log file is `logs/arcadeblocks2.log`. The default settings file is
`settings.json` in the same data directory.

## Tests

Run the full preset test suite:

```bash
ctest --preset linux-debug
```

Run the Catch2 executable directly:

```bash
./build/linux-debug/arcadeblocks2_foundation_tests
```

The CTest suite includes:

- Catch2 foundation, localization, settings, key binding, and boss-level tests.
- `ArcadeBlocksII --version`.
- Headless SDL smoke tests for menu, gameplay, audio-disabled mode, audio dummy
  mode, settings, help, pause, and performance summary routes.
- An expected-failure test for invalid asset directory handling.

## Performance Baseline

`tools/performance_baseline.py` runs deterministic Linux smoke scenarios with
dummy SDL drivers and writes a Markdown report:

```bash
cmake --preset linux-release
cmake --build --preset linux-release --parallel
python3 tools/performance_baseline.py \
  --executable build/linux-release/ArcadeBlocksII \
  --assets-dir assets \
  --frames 180 \
  --output build/performance-baseline-linux.md
```

The script records startup time, frame timing, RSS, texture statistics, asset
load attempts, cache hits, and UI transitions.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `src/core` | Application lifecycle, CLI parsing, logging, version, clock. |
| `src/platform` | Path resolution and SDL runtime integration. |
| `src/render` | Renderer, texture wrapper, sprite atlas, asset manager. |
| `src/audio` | Audio system and sound bank. |
| `src/gameplay` | Main game world and gameplay state. |
| `src/physics` | Box2D integration. |
| `src/levels` | Level types, loader, and repository. |
| `src/settings` | Settings model, persistence, key binding validation. |
| `src/localization` | JSON localization loader. |
| `src/ui` | Menu, HUD, pause, settings, help, and debug views. |
| `assets` | Runtime fonts, sprites, localization, levels, sounds, and music. |
| `tests` | Catch2 test sources. |
| `tools` | Conversion, sprite generation, and performance tooling. |
| `scratch` | One-off asset helper scripts. |
| `ai_temporary` | Temporary/generated art and localization working files. |
| `lbreakouthd-1.2.2` | Imported LBreakoutHD source/assets used as reference material. |
| `cmake` | Shared CMake compiler-option helpers. |

## Asset Notes

The current asset tree contains:

- `assets/levels/arcadeblocks_1`, `assets/levels/arcadeblocks_2`, and
  `assets/levels/classic` JSON level sets.
- More than 3,600 JSON level files.
- `assets/sprites/sprite_atlas.png` with matching atlas metadata.
- English and Russian UI/chapter localization in `assets/localization`.
- Exo 2 and Orbitron fonts.
- OGG music and sound effects for menu, gameplay, levels, bosses, bonuses, and
  UI feedback.

`tools/convert_classic_levels.py` is the conversion utility for classic level
data. Boss sprite generation helpers live in `tools/generate_boss3_sprites.py`
and `tools/generate_boss4_sprites.py`.

## Development Notes

- CMake exports `compile_commands.json` in preset build directories.
- Project warnings are centralized in `cmake/ArcadeBlocksCompilerOptions.cmake`.
- `ARCADEBLOCKS_ENABLE_SANITIZERS=ON` enables AddressSanitizer and
  UndefinedBehaviorSanitizer where supported.
- `ARCADEBLOCKS_WARNINGS_AS_ERRORS=ON` promotes compiler warnings to errors.
- Runtime smoke tests should pass with `SDL_VIDEODRIVER=dummy`; audio tests also
  use `SDL_AUDIODRIVER=dummy`.
- Keep generated build directories, fetched dependencies, local settings, logs,
  and IDE metadata out of version control.
