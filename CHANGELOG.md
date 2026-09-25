# Changelog

## 0.2.0 — 2026-09-25

First public Prism release.

### Interface

- Add the dedicated in-game Prism ClickGUI opened with **Insert**.
- Add compact dark-glass styling, configurable themes, accent colors, opacity and animation timing.
- Add draggable/persistent ClickGUI and HUD layouts with snapping, locking and reset controls.
- Add a color picker with alpha support and optional Windows system cursor behavior in menus.

### Visuals and HUD

- Add configurable Tee and Hook styling and effects.
- Add player trails and configurable visible-player boxes.
- Add modular HUD elements, active-hotkey/client information and input visualization.
- Add live visual/effect preview infrastructure and presets.

### Macros and input

- Add configurable multi-action macros and DDNet-oriented actions.
- Preserve explicit manual/synthetic input ownership so modules release controls cleanly.
- Keep manual input priority where appropriate and reset synthetic state on emergency/reset paths.

### Assist

- Add **Hook Assist**, active only while the player manually uses Hook. It can correct the outgoing hook aim toward an eligible visible tee without moving the physical cursor or initiating Hook itself.
- Add **Freeze Avoid** using bounded client-side prediction.
- Freeze Avoid preserves the player's intended route instead of treating proximity to freeze as failure.
- Add bounded recovery using movement, jump and, when required, synthetic Hook/aim control.
- Improve narrow-passage behavior for low-clearance DDNet/KOG movement.
- Add Assist debug visualization for prediction, hazards, targets and selected recovery paths.

### Build and validation

- Windows x64 is the primary release target.
- Add Debug and Release CI validation.
- Run the automated test suite and packaged-client smoke checks in CI.
- Package source/build provenance, SHA-256 verification data and dependency license notices with release artifacts.
- Keep Prism configuration separate from the normal DDNet settings file.

Prism is an independent modified DDNet distribution and is not an official DDNet release.
