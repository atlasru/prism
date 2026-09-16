# Foundation

- Upstream: https://github.com/ddnet/ddnet
- Selected stable tag: **20.0**
- Exact commit: **a5a61806e434ef22141b622db989087fbba8ed21**
- Prism target: Windows 10/11 x64, v0.1.0.

## History-preserving import

Inspection found an empty `atlasru/prism` repository: GitHub returned HTTP 409 for commits, no branches and no pull requests. An initial README established `main` (`48cb557f6f8a9d1f9d7ca3332e20b6bb03dd2037`). Development uses `feature/prism-v0.1.0-windows`.

Direct Git access from the implementation environment timed out. A one-time Windows Actions job fetched the exact upstream commit, made an unrelated-history merge with `-s ours --no-commit`, then restored the upstream tree before committing. Both histories are parents of import commit `04d647236651e9aa8b9280604ac52d4f0e20c2db`. No history was rewritten. The initial README and Prism workflows were retained; upstream workflows were excluded to keep CI Windows-only. Upstream source, resources, submodule pin and license files were imported unchanged.

Import evidence: https://github.com/atlasru/prism/actions/runs/35081360397

## Architecture

Existing CMake/MSVC, DDNet configuration macros, native UI, renderer and packaging targets are reused. `prism_variables.h` defines saved fields; `prism.h` owns preset logic and defensive validation. Rendering hooks consume interpolated positions; physics, snapshots and network protocol definitions are unchanged. Separate config filename avoids overwriting DDNet settings.

Tee effects draw cached, white alpha silhouettes derived from each loaded skin's original outline textures. Only body/feet silhouettes are accented; decorations, eyes and hats retain upstream rendering. Masks are allocated at skin load and unloaded through the existing skin lifecycle. This adds persistent texture memory even with effects disabled, but no mask creation per frame. Three translucent expanded layers approximate glow; it is not a post-processing blur. Original Tee rendering follows the accent pass unchanged.

Hook tint preserves the existing head and chain geometry. Glow adds three translucent quads along the same interpolated endpoints. Original hand rendering is retained. Ghost/shadow previews are intentionally left on upstream rendering. Demo playback uses the same character path; local/other classification follows the upstream snapshot local ID.

The overlay reuses DDNet's average frame time and cached FPS text container. It follows the existing HUD/video-recording visibility rules and the Prism master switch.

CI packages use upstream's OpenGL backend with Vulkan disabled; the implementation uses only IGraphics abstractions. Vulkan source paths remain intact but are not validated here. Steam/Discord integration and upstream auto-update are disabled in the package. No telemetry is added. Existing DDNet server lists/skin downloads remain upstream functionality.

## Synchronization

```sh
git remote add upstream https://github.com/ddnet/ddnet.git
git fetch upstream --tags
git switch -c update/ddnet-NEXT feature/prism-v0.1.0-windows
git merge --no-ff UPSTREAM_STABLE_SHA
```

Resolve renderer/config/UI changes explicitly, retain the Windows-only workflow policy, update this pinned revision, run Release/Debug tests and the manual matrix, and review via PR. Do not repeat the bootstrap import or squash away upstream ancestry.
