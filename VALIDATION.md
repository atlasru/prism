# Validation

## Recorded baseline

Unmodified DDNet 20.0 code at a5a61806e434ef22141b622db989087fbba8ed21, imported with only README/workflow differences. Both Windows x64 Release and Debug built successfully. Each passed **363 C++ tests** (3 upstream disabled tests). Release additionally ran upstream Rust tests. Baseline distribution uploaded by:

https://github.com/atlasru/prism/actions/runs/35081612143

## Prism Phase 2 automated verification

Source integration ran on Windows via `Apply Prism Phase 2 integration` and asserted every modified code anchor before committing. The Windows CI workflow runs x64 Release and Debug, builds `game-client` and `testrunner`, executes `run_tests`, packages Release, validates distribution and runs a packaged-client smoke test. Record the final Phase 2 run and results from GitHub Actions before treating it as validated; the baseline above is **not** Phase 2 evidence. Static builds cannot establish visual quality, frame-time impact or interactive correctness.

## Phase 2 manual acceptance matrix — not performed

| Area | Procedure | Expected |
|---|---|---|
| Startup | Extract complete ZIP; launch Windows 10/11 x64 and connect to a server | Game, audio and resources load normally |
| Insert modal | Press Insert during play; press Insert again; reopen and press Escape | Overlay opens/closes without opening legacy DDNet Settings or leaving stuck movement input |
| Input focus | Hold a movement/hook key when opening; click menu and sliders, then close | Gameplay inputs released on opening; mouse clicks only affect UI while open |
| Navigation | Select General, Presets, Tee, Hook, Interface and Performance; press Tab | Each section displays functional controls; Tab cycles sections |
| Popups | Change Tee/Hook colors in their picker, close picker, then overlay | No floating picker remains and pointer does not leak to gameplay |
| Persistence | Change every control, exit normally and restart | Values reload from `settings_prism.cfg`; legacy DDNet settings intact |
| Presets | Apply all 5 presets; edit built-in; reset | Built-in editing becomes Custom; Default disables Prism visuals; built-in definitions retain values |
| Interface | Set menu scale 80/100/120%, opacity 50/85/100%, reduced motion 0/1 | Overlay stays on-screen; visual changes apply; reduced motion eliminates transitions |
| Tee | Compare default/custom skins, 0.6/0.7, own/others, jump/freeze and camera zoom | Feathered body/feet outline and glow follow render positions, never hitboxes |
| Hook | Own/others, flying/attached/retracting, moving targets | Tints/halo align with original hook endpoints; reach, timing, physics unchanged |
| Master disable | Set Default or toggle `prism_enabled 0` during gameplay | Tee/Hook appearance returns to original without hiding unrelated DDNet UI |
| Performance HUD | Toggle FPS/frame-time from General and Performance; restart | Same HUD state; saved configuration |
| Visibility | Other-team alpha 0/50/100; spectate; ghost, demos, pausing | Existing alpha rules and ghost exclusions remain |
| Windowing | Test 1280×720, 1920×1080, 2560×1440 and window resize | Readable, scrollable sections and usable color popups |
| Compatibility | Standard DDNet server, dummy, chat, console, editor and demo | No input, gameplay, protocol, or editor regressions |

## Performance — not measured

No average/P95/P99 frame time, FPS, GPU overhead or memory comparison is claimed. Targets (Clean <=5% additional frame time, <=30 MB additional memory) remain unverified.

Reproducible manual procedure:

1. Use baseline and Phase 2 Release artifacts on the same Windows PC, driver, monitor, resolution, OpenGL backend, VSync/FPS cap and DDNet settings. Use copied configs with only Prism fields changed.
2. Play the same locally recorded demo with fixed camera and playback speed; test a crowded scene and a sparse scene. Warm up for 30 seconds.
3. Record 120 seconds of PresentMon frame data for baseline, Prism disabled, Clean, Competitive and Cinematic; repeat three times. Avoid menus/background tasks.
4. Calculate arithmetic mean, P95 and P99 of per-frame milliseconds, FPS = 1000/mean, and relative frame-time delta. Sample process private working set and GPU dedicated/shared memory every second. Report hardware, command/tool version, demo hash and raw measurements.
5. Compare medians across repeats and retain variance. Do not substitute CI/software-renderer startup times for gameplay benchmarks.

## Scope limitations

Frosted glass is layered translucent UI, not framebuffer blur. Glow is a bounded multi-layer approximation; Tee silhouettes do not recolor hats/decorations. Mask textures remain allocated when disabled. CI distribution uses OpenGL; Vulkan and non-Windows platforms are not validated. No production tag/release or automatic merge is authorized.
