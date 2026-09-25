# Prism v0.2.0

Prism v0.2.0 is the first public release of the Windows-focused DDNet client.

## What's included

- Dedicated **Insert** ClickGUI with Prism's dark-glass visual identity.
- Configurable Tee/Hook visuals, effects, trails and visible-player boxes.
- Modular HUD, hotkey/client information and input visualization.
- Draggable layouts, snapping, lock/reset controls, themes, alpha-aware color picking and configurable animations.
- Multi-action macro support with explicit input ownership.
- **Hook Assist** that only assists while you manually use Hook.
- **Freeze Avoid** with bounded prediction and route-preserving recovery using movement, jump and Hook/aim when required.
- Improved behavior in narrow, low-clearance DDNet/KOG passages.
- Separate Prism configuration so normal DDNet settings are not overwritten.

## Validation

The release workflow builds and tests both Windows Debug and Release configurations before publishing. The Release package is additionally reopened and smoke-tested, and ships with build provenance, file hashes and dependency license notices.

## Notes

- Target: Windows 10/11 x64.
- Builds are currently unsigned, so Windows may show an unsigned-app warning.
- Prism is under active development; behavior and configuration can change in later releases.
- Prism is an independent modified DDNet distribution and is not an official DDNet release.
