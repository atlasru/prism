# Atmosphere and Player Glow

Branch: `feature/prism-atmosphere-glow`. This patch does not change the release version.

Both features default to **off**. Enable Prism visuals in the Insert menu, then enable the desired effect under Visuals. Existing Tee outlines/glow, hooks, trails, highlights, HUD, assists and macros retain their settings and implementation.

## Rendering

Atmosphere uses the existing threaded DDNet graphics command path and desktop OpenGL 3.3 backend. The completed world framebuffer is copied into one persistent RGBA8 screen texture, then a single fullscreen triangle applies color grading and optional bloom. No framebuffer switching, CPU readback or per-frame GPU allocations. Shaders compile at context initialization. The screen texture allocates on first use and reallocates only on viewport size changes; disabling the effect sends no post-process command. Texture memory is `width × height × 4` bytes (7.91 MiB at 1920×1080). Allocation/compilation failure bypasses the effect.

The pass runs after map foreground and explosion particles, before nameplates. HUD, menus, text, freeze bars and cursor remain unprocessed. Particle groups which render after nameplates also remain unprocessed. This preserves DDNet's component order. Screenshots capture the same processed world and unprocessed UI as the screen. Menu background scenes are never processed; opening the menu while playing can still show the processed world behind it.

Exposure uses `exp2(stops)`. Contrast is centered at 0.5; saturation uses Rec.709 luminance. Highlights/shadows are luminance-weighted adjustments, not physical lighting or ambient occlusion. Gamma applies a bounded positive power to clamped channels. Tint multiplies RGB, with strength multiplied by the picker alpha. Vignette darkens screen edges. Processing operates on display RGB, without HDR/linear-light conversion.

Bloom is a **nine-tap, thresholded single-pass approximation**, including a 4:2:1 weighted center/cardinal/diagonal kernel. There is no blur pyramid, reduced-resolution buffer, or extra framebuffer. Strength zero skips bloom sampling. Threshold 1 produces no bloom from the LDR source. Wide radius remains the same fixed tap count and can reveal separate lobes around tiny bright objects; this is a performance tradeoff, not cinematic Gaussian bloom.

Player Glow uses six gradient rings, 32 sectors and additive blending before each normally rendered Tee. It has no textures, screen blur or framebuffer. Storage is on the stack. Grid traversal clips radial geometry against solid collision tiles (bounded to 32 traversal steps per ray), and map foreground subsequently covers it. This is a radial polygon approximation, not per-pixel occlusion against arbitrary decorative layers. Six rings can show bands under unusually sharp falloff. Transparent Tees and custom-color alpha multiply aura opacity. Ghosts are excluded. Local, connected dummy and other player paths are mutually exclusive. The dummy toggle controls the connected secondary Tee; the currently controlled Tee uses Local. Spectating/demo playback reuse the normal visible-player render path.

OpenGL 1.x/2.x/3.0, OpenGL ES and Vulkan keep normal rendering and saved Atmosphere settings, with an explicit unsupported message in the menu. Glow uses existing untextured geometry and is independent of the post-process backend. No unsupported post-process command reaches Vulkan.

## Settings

Percent settings use integer percentages in config; exposure uses hundredths of a stop. All settings use `CFGFLAG_SAVE`, existing DDNet serialization and native range clamping, with an additional bounded render/config validation layer.

| Setting | Range | Default |
|---|---:|---:|
| `prism_atmosphere` | 0–1 | 0 |
| `prism_atmosphere_preset` | 0–4 | 0 |
| `prism_exposure` | −400–400 (−4–4 stops) | 0 |
| `prism_contrast` | 0–400 (0–4×) | 100 |
| `prism_saturation` | 0–400 (0–4×) | 100 |
| `prism_gamma` | 25–400 (0.25–4) | 100 |
| `prism_highlights` | −100–100 | 0 |
| `prism_shadows` | −100–100 | 0 |
| `prism_bloom_strength` | 0–500 (0–5×) | 0 |
| `prism_bloom_threshold` | 0–100 | 80 |
| `prism_bloom_radius` | 1–32 screen pixels | 4 |
| `prism_vignette` | 0–100 | 0 |
| `prism_tint_strength` | 0–100 | 0 |
| `prism_atmosphere_tint` | packed HSLA, full picker alpha | opaque white |
| `prism_player_glow` | 0–1 | 0 |
| `prism_glow_local` | 0–1 | 1 |
| `prism_glow_others` | 0–1 | 0 |
| `prism_glow_dummy` | 0–1 | 1 |
| `prism_glow_intensity` | 0–400 (0–4×) | 100 |
| `prism_glow_radius` | 8–192 world units | 48 |
| `prism_glow_alpha` | 0–100 | 25 |
| `prism_glow_softness` | 25–800 (exponent 0.25–8) | 200 |
| `prism_glow_color_mode` | 0 Accent, 1 Entity body, 2 Custom | 0 |
| `prism_glow_color` | packed HSLA, full picker alpha | opaque white |

Exposure bounds give a 1/16–16× multiplier. Gamma cannot reach zero or a singular power. Contrast/saturation/intensity bounds permit aggressive styles while retaining bounded output and RGBA8 precision. Alpha, tint, vignette and threshold span their complete normalized domain. Glow radius is bounded to six tiles to control overdraw and collision work. Bloom radius is bounded to the useful local neighborhood of the sparse nine-tap kernel; greater radii increase separated-lobe artifacts. Final RGB and glow vertex alpha are clamped to 0–1.

Color config defaults: `4278190335` (packed opaque white HSLA). Hex console colors use **RGBA**, e.g. `prism_glow_color $55AAFF80` for alpha 128/255.

## Presets and persistence

`prism_atmosphere_preset <id>` applies a preset. The saved `prism_atmosphere_preset` config variable records identity and does not apply values by itself.

| ID / name | Exposure | Contrast | Saturation | Highlights / shadows | Bloom strength / threshold / radius | Vignette |
|---|---:|---:|---:|---:|---:|---:|
| 0 Off | preserved | preserved | preserved | preserved | preserved | preserved |
| 1 Subtle | 0 | 105 | 105 | 0 / 0 | 10 / 80 / 4 | 0 |
| 2 Cinematic | −30 | 125 | 80 | −15 / −10 | 35 / 80 / 8 | 25 |
| 3 Vivid | 0 | 115 | 150 | 0 / 0 | 25 / 80 / 4 | 0 |
| 4 Custom | preserved | preserved | preserved | preserved | preserved | preserved |

Named presets enable Atmosphere, set gamma to 100 and tint strength to 0. They preserve the tint color and all Glow settings. Off disables Atmosphere without erasing values. Custom preserves every manual value; it does not resurrect an older saved custom profile after a named preset overwrote grading values. Manual changes in either UI or console mark Atmosphere Custom. Identity serializes last so named presets survive restarts after their values have loaded. Old configs lack the new keys and therefore start with both systems disabled; no migration rewrites or key renames occur.

## Validation and performance evidence

The Windows workflow builds Debug and Release, runs the complete current test suite, packages Release and checks real-process config persistence across restarts, including legacy OpenGL fallback. Consult the branch's Actions run for the tested commit and final results.

Deterministic tests cover exact defaults, every console range bound, direct invalid values, named preset mapping, Custom/Off preservation, unrelated Glow/gameplay preservation, serialization with picker alpha, disabled world paths, exclusive dummy targeting, color mapping, monotonic finite falloff, solid-wall/corner/narrow-corridor clipping and bounded ray traversal.

The actual GLSL compiled and linked on `llvmpipe (LLVM 20.1.2, 256 bits)`, OpenGL `4.5 (Core Profile) Mesa 25.2.8-0ubuntu0.24.04.2`. EGL shader QA passed at 1×1, 65×37, 320×180 and 800×600. Neutral grading preserved orientation/alpha/RGB with maximum float sampling error `0.00010699033737182617`. Desaturation and both parameter extremes produced finite, bounded output. This is isolated shader validation, **not gameplay validation or a hardware FPS measurement**.

No game-client FPS or frame-pacing figures are claimed. The local environment lacks the dependencies/display needed for a complete interactive DDNet session. `scripts/benchmark_prism_visuals.py` uses DDNet's existing `benchmark_quit` timer against the same recorded demo for baseline, Glow, Atmosphere, bloom, combined and aggressive profiles. It reports measured average FPS, mean/p95 frame time and relative overhead, excludes a five-second warmup, uses isolated saved profiles and rejects legacy OpenGL fallback. Use a demo longer than 26 seconds:

```sh
python scripts/benchmark_prism_visuals.py path/to/Prism.exe path/to/scene.demo
python scripts/check_prism_shader.py
```

The EGL script requires `moderngl` and `numpy`; they are optional QA dependencies, not client dependencies.

## Manual validation checklist — pending

- Dark, bright, freeze-heavy and high-contrast maps: compare off/on and extreme grading; verify freeze readability and banding.
- Multiple players, local/dummy switching, hook active, spectating and demo playback: check targeting, alpha stacking and duplicate auras.
- Solid corners/narrow corridors and decorative foreground: check aura clipping and no visible halos through walls.
- Menu open, HUD/nameplates/freeze bars, cursor and UI scale: check unprocessed UI and no state leakage.
- Window resize, fullscreen/windowed transitions, map transitions and context restart: check texture dimensions, flicker and stale content.
- Screenshots and MSAA: compare screen/capture, alpha and resolve behavior.
- Run all six benchmark profiles on the same scene and real Windows GPU; check mean/p95 frame time and stutter.

## Changed files

- `src/engine/shared/prism_atmosphere_variables.h`, `config_variables.h`: persistent settings and safe ranges.
- `src/game/client/prism_atmosphere.h`: presets, validation, color/target logic and bounded collision-grid clipping.
- `src/game/client/components/menus_settings_prism.cpp`, `players.cpp`, `gameclient.cpp`: existing menu, player aura and world/UI render boundary.
- `src/engine/graphics.h`, `src/engine/client/graphics_threaded.{h,cpp}`, `backend_sdl.h`: optional capability and render-thread command.
- `src/engine/client/backend/opengl/backend_opengl{,3}.{h,cpp}`: shader lifecycle, reusable texture, copy/pass and graphics-state restoration.
- `data/shader/prism_atmosphere.{vert,frag}`: fullscreen triangle, grading and lightweight bloom.
- `src/test/prism_test.cpp`, `scripts/smoke_prism.py`: deterministic and process-restart checks.
- `scripts/check_prism_shader.py`, `scripts/benchmark_prism_visuals.py`: reproducible shader QA and real-client measurement tools.
- `CMakeLists.txt`, `.github/workflows/prism-windows.yml`, this document: shader packaging, branch CI and implementation/validation details.
