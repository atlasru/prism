# Prism Phase 3 — validation and acceptance

Phase 3 builds on `feature/prism-overlay-ui`. Its gameplay UI and GPU rendering require an interactive Windows acceptance pass. A successful automated build or startup smoke is **not** a substitute for that pass.

## Automated acceptance

- [ ] Windows x64 Debug: `game-client`, `testrunner`, and `run_tests` succeed.
- [ ] Windows x64 Release: same targets/tests succeed.
- [ ] Focused `PrismMacro.*` and `PrismHud.*` tests pass alongside Phase 2 `PrismConfig.*` regression tests.
- [ ] Release `package_default` and `scripts/package_prism.py build` complete; generated ZIP CRC, entry hashes, PE imports, package contents and archive SHA-256 verified.
- [ ] `scripts/smoke_prism.py build/verified-package/Prism` completes six process runs testing startup/config persistence.
- [ ] No changes in server sources, game physics or network protocol; compare PR diff to Phase 2 base.

## Interactive Windows acceptance (must be performed before merging or releasing)

1. **Overlay:** Connect to a DDNet server. Insert opens/closes Control Center. Escape closes; Tab traverses all seven sections. Check mouse clicks, scrolling, color picker, keybind capture, reduced-motion settings, viewport changes and focus alt-tab. Check normal legacy game binds after overlay closes.
2. **Double Tee:** Connect dummy. Check Copy, Hammer Fly and Combined separately. Confirm simultaneous movement and hammer attacks in Combined with manual direction, hook and jump. Test interval 5, 25 and 100 ticks. Toggle activation key; press F12 and custom emergency key while controls are held. Check deactivation, dummy disconnect/reconnect, player/dummy swap, menu opening and alt-tab. Verify all Prism-owned movement, hook, jump and fire are released while manually held controls remain active. Verify Hammer Fly only aims/fires using connected, valid player IDs.
3. **Macros:** In each of four slots, use Add/Delete/Move and Action/Delay controls to build multi-action sequence. Bind unique key. Check Once, Hold and Toggle, release during a delay, repeated key events, editing while running, conflicting macros, manual takeover, emergency stop, menu open, alt-tab, and disconnect/reconnect. Verify **no sticky input** and no external shell/program/script execution. Restart client and verify exact editor contents, binds, modes and enable flags persist.
4. **HUD:** Toggle every module independently and the global enable. Change HUD scale/opacity. Drag each enabled module in preview, including all edges; restart and change window resolution/aspect ratio. Verify normalized layout persists and stays on screen. Reset layout; verify expected defaults. Check Active Hotkeys matches actual state, identity/version matches build, performance displays reasonable FPS/frame time, dummy connected/offline states follow connection, and no staff guesses are made when the server omits auth metadata. Only explicit server-provided auth levels may identify privileged players.
5. **Visual regression:** Cycle Default/Clean/Competitive/Cinematic/Custom, Tee and Hook local/other adjustments. Confirm preset edits become Custom and do not overwrite Double Tee, macros or HUD configuration. Toggle Prism visuals off; vanilla DDNet visuals return. Re-check Phase 2 overlay transparency, scale, animations and reduced-motion.
6. **Compatibility:** Connect with and without dummy, play several maps, test spectating, local server, demos, chat, console, menus, and disconnect. Confirm no modification to game physics, packets or anti-cheat behavior.
7. **Performance:** Compare FPS, average frame time, 1%-low frame time and process memory on the same map and hardware against Phase 2 with HUD/macros off/on. Test 60/120/144+ FPS, integrated GPU, and different window sizes. Avoid declaring a performance regression absent comparable measurements.

## Scope and limitations

- The macro editor supports four slots and at most eight whitelisted actions each. Each delay is 0–5000 ms. It intentionally does not support external processes, scripting, arbitrary commands or user-provided code.
- Staff status is limited to explicit server-provided authentication metadata. Without it, the UI reports unavailable; it never infers moderation status from names, appearances or third-party lists.
- UI frosted glass is layered translucency rather than full framebuffer blur. Vulkan and non-Windows builds are outside Windows CI scope.
- Visual/pointer correctness, gameplay execution and measured performance remain manual acceptance requirements; automated tests cover isolated state and packaging only.
