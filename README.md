# Prism

**Prism** is an open-source Windows client based on [DDRaceNetwork (DDNet)](https://github.com/ddnet/ddnet), focused on visual customization, quality-of-life tools and optional gameplay assistance while keeping the DDNet experience familiar.

Prism is developed as a client-side extension of DDNet. The project keeps the upstream game, networking, maps, demos and editor while adding its own interface and feature set.

## Highlights

### Prism interface

Press **Insert** in-game to open Prism's dedicated ClickGUI.

The interface uses a compact dark-glass design with configurable accent colors, opacity and animation timing. Prism has its own theme and layout system instead of placing all custom options inside the standard DDNet settings pages.

The UI includes draggable and persistent layouts, snapping, layout locking/reset, a color picker with alpha support, and an optional Windows system cursor in menus.

### Visuals

Prism provides configurable client-side visual effects, including:

- Tee outlines and glow;
- Hook customization and effects;
- player trails;
- configurable visible-player boxes;
- effect previews;
- separate local and other-player styling;
- presets and theme customization.

Visual effects do not modify DDNet physics or server state.

### HUD

Prism includes a modular HUD system with configurable widgets such as active hotkeys, client/version information, performance information and input visualization.

HUD elements can be positioned and configured independently.

### Macros

Prism includes a configurable macro system for multi-action input sequences and DDNet-oriented actions.

Manual input retains priority where appropriate, and Prism keeps explicit ownership of synthetic inputs so modules do not leave movement, fire or hook state stuck after they stop.

### Assist

Prism includes optional Assist modules built on bounded client-side prediction.

**Hook Assist** activates only while the player manually uses Hook. It can assist the outgoing hook direction toward an eligible visible tee while preserving the player's physical cursor and never initiating Hook by itself.

**Freeze Avoid** predicts the player's short-term trajectory and can intervene when a trajectory is expected to enter freeze/death. It is designed to preserve the player's intended route rather than simply maximizing distance from freeze. Recovery can use bounded movement, jump and hook/aim corrections when required.

Assist can be disabled completely and includes debug visualization for testing prediction and selected recovery paths.

## Performance

Prism's real-time systems are designed around bounded prediction and fixed candidate sets rather than unbounded searches. Expensive diagnostic rendering is optional.

The client retains DDNet's normal rendering/gameplay architecture and is intended to remain usable at high frame rates.

## Windows

Windows x64 is the primary supported platform.

Download a packaged build from a successful **Prism Windows** GitHub Actions run, extract the complete archive and launch `Prism.exe`.

Keep the bundled `data` directory, DLLs and license files beside the executable.

Prism builds are currently unsigned, so Windows may display the usual warning for an unsigned executable.

## Configuration

Prism settings use DDNet's configuration infrastructure but are stored separately in:

`settings_prism.cfg`

On Windows this is normally located in DDNet's user directory under `%APPDATA%/DDNet`.

Prism does not replace `settings_ddnet.cfg`.

## Building

Requirements include Git, CMake, Python 3, Rust and Visual Studio 2022 with the C++ desktop workload and Windows SDK.

```powershell
git clone --recurse-submodules https://github.com/atlasru/prism.git
cd prism
git submodule update --init --recursive

cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=. -DDOWNLOAD_GTEST=ON -DVULKAN=OFF -DAUTOUPDATE=OFF -DSTEAM=OFF -DDISCORD=OFF -DTOOLS=OFF
cmake --build build --config Release --target game-client testrunner --parallel 4
cmake --build build --config Release --target run_tests
cmake --build build --config Release --target package_default
python scripts/package_prism.py build
```

The repository's Windows CI builds and validates both Debug and Release configurations and performs packaged-client smoke testing.

See [CONTRIBUTING.md](CONTRIBUTING.md), [FOUNDATION.md](FOUNDATION.md) and [VALIDATION.md](VALIDATION.md) for additional project information. Upstream DDNet documentation remains available in `docs/`.

## Project status

Prism is under active development. Features and configuration may change between builds.

The `main` branch contains the current integrated Prism codebase. Development work may still happen in feature branches before being merged into `main`.

## Attribution and license

Prism is an independent modified DDNet distribution and is **not an official DDNet release**.

DDNet and Teeworlds retain their respective copyrights. Prism code follows the applicable upstream licensing; bundled assets, fonts, skins and third-party libraries may have their own licenses.

See `license.txt` and the bundled license notices for details.
