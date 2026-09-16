# Prism

Prism is an open-source Windows 10/11 x64 client based on **DDRaceNetwork (DDNet) 20.0**. Version 0.1.0 is under review; this is not a production release.

## Features

- Native **Settings → Prism** page with General, Tee, Hook and Performance categories, dark neutral panels and scrollable controls.
- Separate local/other-player Tee outlines (color, width) and optional layered glow (color, intensity). Supports 0.6 and 0.7 body/feet skin silhouettes.
- Separate local/other-player hook tint and optional glow, using the original interpolated endpoints.
- Default, Clean, Competitive, Cinematic and Custom presets. Editing a built-in preset selects Custom.
- Immediate global visual toggle and reset. Optional FPS / average frame-time display using DDNet timing.
- Existing DDNet navigation, gameplay, protocol, demos and editor retained. No automated gameplay or new network data.

## Windows installation

Download the ZIP from a successful **Prism Windows** GitHub Actions run. Extract the **entire archive**, then start `Prism.exe` inside the extracted folder. Keep `data`, DLLs and license notices beside it. Windows 10/11 x64 and an OpenGL-capable graphics driver are required. CI packages use OpenGL, not Vulkan. Executables are unsigned.

Prism starts with original DDNet visuals. Select Clean or enable effects under Settings → Prism. `prism_toggle` in F1 toggles effects; `bind f8 prism_toggle` assigns a shortcut without replacing any binding automatically. `prism_apply_preset 0` restores Default; IDs 1–4 select Clean, Competitive, Cinematic and Custom. `prism_reset` resets Prism only.

Settings use DDNet's configuration system and are saved as `settings_prism.cfg` in DDNet's normal user directory (normally `%APPDATA%/DDNet` on Windows). `settings_ddnet.cfg` is not overwritten. Assets and demos can remain shared. Existing `autoexec` files still run as in DDNet.

## Build

Requires Git, CMake, Python 3, Rust (at least 1.85) and Visual Studio 2022 with Desktop development with C++ and Windows SDK.

```powershell
git clone --recurse-submodules https://github.com/atlasru/prism.git
cd prism
git switch feature/prism-v0.1.0-windows
git submodule update --init --recursive
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=. -DDOWNLOAD_GTEST=ON -DVULKAN=OFF -DAUTOUPDATE=OFF -DSTEAM=OFF -DDISCORD=OFF -DTOOLS=OFF
cmake --build build --config Release --target game-client testrunner --parallel 4
cmake --build build --config Release --target run_tests
cmake --build build --config Release --target package_default
python scripts/package_prism.py build
```

See [CONTRIBUTING.md](CONTRIBUTING.md), [FOUNDATION.md](FOUNDATION.md) and [VALIDATION.md](VALIDATION.md). Upstream documentation remains in `docs/`.

## Attribution and license

Prism is a modified DDNet distribution, not an official DDNet release. DDNet and Teeworlds retain their copyrights. Prism additions use the upstream zlib license. See `license.txt`; assets, fonts, skins and bundled libraries have separate licenses. The package retains upstream notices and adds a `licenses/` collection. Original DDNet icons remain in this initial version.
