# Validation

## Recorded baseline

Unmodified DDNet 20.0 code at a5a61806e434ef22141b622db989087fbba8ed21, imported with only README/workflow differences. Both Windows x64 Release and Debug built successfully. Each passed **363 C++ tests** (3 upstream disabled tests). Release additionally ran upstream Rust tests. Baseline distribution uploaded by:

https://github.com/atlasru/prism/actions/runs/35081612143

## Prism validation status

Implementation validation is in progress. Do not interpret the baseline result as a successful Prism build. Final CI evidence will be recorded after completion. Artifact-specific commit/source hashes, file inventory and SHA-256 are produced by the packaging workflow. No manual visual test or GPU benchmark has been performed.

## Manual acceptance matrix — not performed

| Area | Procedure | Expected |
|---|---|---|
| Startup | Extract complete ZIP; launch on Windows 10 and 11 x64 | Menu, audio and resources load |
| Settings | Change every control; restart | Values persist; no empty controls |
| Default / toggle | Cycle presets, disable Prism while playing | Original Tee/hook rendering immediately restored |
| Custom | Edit built-in values, restart, reselect built-in | Custom persists; built-in definition unchanged |
| Tee | Default/custom skins, 0.6/0.7, movement/jump/freeze, zoom | Body/feet outlines follow interpolated skin, no state changes |
| Hook | Local/others, flying/attached/retracting, moving targets | Accents align; reach/timing unchanged |
| Visibility | Other-team alpha 0/50/100, spectators, ghosts | Existing visibility respected; ghosts unchanged |
| Demo | Record/play/seek/pause with multiple players | Effects follow shared render path without editing demo data |
| Overlay | Toggle Prism/overlay/HUD/recording | FPS and ms follow existing HUD rules |
| UI | 1280×720, 1920×1080, 2560×1440; several UI scales | Scrollable controls, readable tabs and color pickers |
| Compatibility | Standard DDNet server; dummy, chat, editor, demo | Existing behavior preserved |

## Performance — not measured

No average/P95/P99 frame time, FPS, GPU overhead or memory comparison is claimed. Targets (Clean <=5% additional frame time, <=30 MB additional memory) remain unverified.

Reproducible manual procedure:

1. Use the baseline artifact above and Prism Release on the same Windows PC, driver, monitor, resolution, OpenGL backend, VSync/FPS cap and DDNet settings. Use copied configs with only Prism fields changed.
2. Play the same locally recorded demo with fixed camera and playback speed; use a crowded scene and a sparse scene. Warm up for 30 seconds.
3. Record 120 seconds of PresentMon frame data for baseline, Prism disabled, Clean, Competitive and Cinematic; repeat three times. Avoid menus/background tasks.
4. Calculate arithmetic mean, P95 and P99 of per-frame milliseconds, FPS = 1000/mean, and relative frame-time delta. Sample process private working set and GPU dedicated/shared memory every second. Report hardware, command/tool version, demo hash and raw measurements.
5. Compare medians across repeats, retaining variance. Do not substitute CI/software-renderer startup times for a gameplay benchmark.

## Scope limitations

Glow is a layered approximation; body/feet silhouettes do not recolor hats/decorations. Mask textures remain allocated when disabled. CI distribution uses OpenGL; Vulkan and non-Windows platforms are not validated. No production tag/release or automatic merge is authorized.
