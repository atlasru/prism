# Intent Pathfinder, weapon profiles and verified unfreeze

Branch: `feature/prism-intent-pathfinder`.
Base: `f6eb4011e5e40494e45410479ccc320292fb3e1c` (`feature/prism-atmosphere-glow`).
The existing Prism client, Atmosphere work and legacy Freeze Avoid are retained.
No merge, release, UI redesign or Auto TAS is included.

## Planner and Freeze Avoid

`CIntentPlanner` produces a local corridor from physically simulated input
sequences, rather than assuming adjacent tiles are reachable. `CPredictor`
executes DDNet character-core Tick/Move/Quantize with collision, tuning, jump and
hook state. No separate movement constants or tile-grid A* are used.

Intent comes from physical direction first, then momentum, recent direction,
recent displacement, Jump and physical Hook/aim. The planner simulates the
manual continuation first. If it is safe, that path commits immediately.
Otherwise it tests immediate/delayed jumps, local hook anchors and short
brake/reverse-then-resume sequences. The endpoint and up to 32 samples describe
the selected corridor; velocity and jump/hook state remain in the trajectory.
The configurable horizon is 50–150 ticks (1–3 seconds at the actual tick rate).

The existing 4–24 tick Freeze Avoid prediction remains the intervention gate:
a safe manual trajectory is never changed, including near adjacent freeze.
A dangerous trajectory is evaluated against route membership and progress,
intervention cost and previous action retention. Clearance is secondary.
Safe emergency candidates remain available when the requested corridor cannot
be followed. Recovery retains the earlier destination and scores rejoining it;
manual intent changes can cancel that commitment. The legacy correction selector
and its hysteresis handle missing/unknown/over-budget planner results.

`BeginCore`, `SimulateSequence` and `SimulateInputs` expose snapshot-based
re-simulation. The latter accepts up to 150 actual per-tick inputs and uses the
same core simulation loop. These are building blocks for future recording and
action search; they are not TAS recording, playback, rewind or a global planner.

## Budgets and debug

- At most 16 planning candidates and 2,400 simulated ticks per search.
- Default planner deadline: 1,500 microseconds; configurable 250–4,000.
- Predictor checks deadline every four ticks; timing uses the uncached steady
  clock, not DDNet's cached frame timer. A deadline remains a soft real-time
  bound: a collision/core step already executing cannot be preempted.
- Cached corridor reused for fewer than five physics ticks while intent, state
  and deviation remain compatible. Collision already owns static map data;
  no full-map search or new full-map cache is built each frame.
- Weapon/rescue searches run once per physics tick, with a shared 2,500 us soft
  deadline and at most eight aim targets. Rescue search starts at most once
  every five ticks. Full-world verification rejects unsupported entity types
  and worlds containing more than 128 entities.
- Movement search uses fixed arrays. Rare enabled rescue/bounce verification
  needs bounded full-world copies and entity allocations to run real weapon
  mechanics. It does not clone worlds in the render path or when disabled.

`prism_path_debug 1` shows the route, corridor boundaries, waypoint, confidence,
mode, average/worst search time, candidate count, cache hits and deadline misses.
Freeze debug still shows manual/corrected/rejected trajectories and hook anchors.
Weapon/rescue debug shows outgoing solver aim and confirmed rescue state.
All debugging defaults off. Confidence is a conservative heuristic, not a
calibrated probability of success.

## Weapons

Each of Hammer, Gun, Shotgun, Grenade and Laser has independent saved enable,
activation, FOV, range, prediction, correction limit, smoothing, priority and
debug settings. Controls use the existing Assist tab and a weapon selector.
Fields that do not apply to instantaneous laser aiming are hidden there.
Defaults are off; existing configuration names retain their meaning.

- Hammer uses the actual character/spawn/radius geometry and one-tick target
  motion estimate. Aim assistance never creates a fire press.
- Gun and vanilla Shotgun solve interception against `CalcPos`, including
  projectile spawn offset, tuned speed/curvature/lifetime and target motion.
- DDRace Shotgun uses the actual laser implementation, not a projectile model.
- Grenade solves the tuned parabola and checks segmented collision feasibility.
- Laser checks reach/collision and the real `CLaser` target hit; optional bounded
  bounce candidates are confirmed by that same implementation.
- Ninja is a dash and deliberately has no generic aim solver/profile.

Target retention and bounded interpolation follow manual aim changes without
moving the physical cursor. Blocked solutions are rejected. Projectile target
motion is a linear estimate; it does not infer another player's future inputs.
Strong smoothing may conservatively reject a partial outgoing correction when
that correction still intersects geometry.

## Auto Unfreeze and ownership

Self and Others have independent saved switches. Others additionally has
physical-Fire/automatic activation, FOV, range, closest-aim/nearest target
selection and a cooldown. Only eligible frozen teammates are selected.

DDRace Hammer and Laser can unfreeze another player. Shotgun, Gun and Grenade
are not treated as unfreeze weapons. A frozen shooter cannot fire. Self mode
therefore tests a laser bounce fired *before* an impending freeze, while the
shooter can still fire, and only accepts a predicted later self-unfreeze.
It does not pretend that pressing Fire while already frozen can rescue self.
Old-laser self hits, deep/live freeze, reload/ammo failures, unsupported
prediction and unconfirmed outcomes produce no automatic action.

`CShotPredictor` compares a no-shot world against a shot world running actual
FireWeapon, CLaser, tile and Unfreeze code. Rescue must produce at least two
consecutive predicted unfrozen ticks while the control world remains frozen.
This rejects shots that immediately return to continuing freeze. Detached copies
preserve live prediction parent/child links and do not mutate source characters.

Aim priority is macro/physical Hook/Freeze Avoid-owned Hook, then verified
Auto Unfreeze, then the current weapon profile, then manual aim. Menu/chat,
unfocused window, spectator and demo gates remain `PrismInputAllowed`.
Macros' direction, jump and hook ownership still constrains Freeze Avoid.
Pending explicit weapon switches defer weapon/rescue decisions. Hammer rescue
respects the physical press edge; holding Hammer Fire does not invent repeat
presses. Verification receives a copy-composed wire fire counter. All automatic
fire edges go through the existing per-connection `CFireComposer`, with one
confirmed pulse and at least configured/weapon reload cooldown. F12/emergency
stop disables new corrective features and clears planner/decision ownership.
Dummy swaps and client resets also clear decision state.

## Validation

Linux Release client builds with GCC 13, Rust 1.85.1, SDL 2.30.0 and system GTest
1.14.0. A separate headless client supports actual restart/persistence checks;
it does not imply visual or gameplay testing.

- Main C++ suite: 411 executed, 410 passed, one environment failure; three
  existing tests remain disabled. No existing test was weakened or excluded.
- Environment failure is the unchanged Unix socket creation test under this
  execution environment's Unix socket restriction:

  ```text
  /DDNet/src/test/unix_test.cpp:9: Failure
  Expected: (Socket) >= (0), actual: -1 vs 0
  ```
- Prism unit subset: 54 passed, including independent profiles, gating,
  projectile interception, rescue cooldown and ownership.
- Separate client physics suite: 18 passed. It covers safe straight travel, low clearance,
  one-tile corridors/openings, dangerous manual input with reachable jump,
  impossible routes, timeout fallback, route rejoin, real weapon collision,
  detached worlds, laser/hammer rescue, valid preemptive self-bounce, source
  state isolation, per-tick replay determinism and planner performance.
- Rust: 10 unit tests and eight doc tests passed.
- Startup/config smoke: 28 actual headless process runs passed, including five
  consecutive reloads of independent weapon/planner/unfreeze settings, range
  clamping, emergency stop and all existing appearance checks.

The final Linux Release benchmark of 1,000 calls per scenario measured:

| Scenario | Actual plans | Cache hits | Mean search | Worst search | Candidates | Simulated ticks (last search) |
|---|---:|---:|---:|---:|---:|---:|
| Safe straight | 200 | 800 | 88.735 us | 190 us | 1 | 101 |
| Freeze jump/recovery search | 200 | 800 | 342.155 us | 1393 us | 11 | 399 |

No deadline misses occurred in those runs. These are synthetic local fixtures,
not gameplay FPS measurements or a guarantee on the user's laptop. The physics
test prints reproducible `PRISM_BENCH` lines; CI retains these in its logs.

Windows CI builds both Debug and Release, runs the complete available C++/Rust
suite and the separate client-physics suite, packages Release, verifies the
triggering commit and runs real packaged-client restart/config smoke. Consult
the branch's **Prism Windows** run and `TESTED_COMMIT.txt` for exact tested SHA,
results, XML totals and artifacts. Final run status is reported with delivery.

## Remaining limits

No manual gameplay or visual comparison has been performed. Static character
core planning cannot predict other players' future inputs, server modifications,
teleports, speedups, tune/switch transitions or tile-granted abilities; unknown
core outcomes disable intervention. Rescue requires supported DDRace tile,
freeze and weapon prediction. Geometry search is local and finite, so valid
complex routes or multi-bounce rescues can be missed. The corridor's radius is
an intent-scoring tolerance, not a claim that the entire band is collision-free.
Only simulated states establish candidate safety. No global navigation or TAS
UI/control features are implemented.

## Manual gameplay checklist

Use an offline/local DDRace map first. Keep the same visual settings and compare
`prism_pathfinder 0` with `1`. Enable `prism_freeze_avoid 2`,
`prism_freeze_debug 1`, `prism_path_debug 1` for diagnostics.

1. Hold right/left through open floor, near freeze and a one-tile-high passage.
   Safe manual input must remain unchanged; adjacent freeze alone must not
   trigger direction/jump/hook intervention.
2. Traverse a one-tile opening with momentum and a low-clearance jump. Check
   that route continuation is preferred to braking or retreating.
3. Approach a freeze gap that needs a timed jump or Hook through an opening.
   Verify the smallest correction only when the manual simulation is dangerous.
   Check physical Hook, an already attached Hook and synthetic recovery Hook.
4. Make the intended continuation impossible. Check bounded per-input recovery,
   stable choice and rejoin toward the earlier waypoint once possible. Repeat
   between equivalent alternatives; watch for rapid left/right or hook retargets.
5. Set `prism_path_budget 250`, cross switch/tune/speedup/teleport sections and
   disable Pathfinder mid-run. Verify UNKNOWN/BUDGET fallback and retained legacy
   Freeze Avoid behavior. Restore the normal budget afterward.
6. Enable one weapon profile at a time. Check activation only on physical Fire
   versus Always, distinct persisted FOV/range, target switching, moving targets,
   walls, Grenade arcs and optional Laser reflection. Gun with Jetpack must not
   use projectile aim correction. Confirm cursor never moves physically.
7. Put a same-team player in timed freeze, outside a freeze tile, within Hammer
   or Laser reach. Enable Others; test Hold Fire and Automatic. Check rescue,
   cooldown, blocked shots, another team, deep/live freeze and no valid target.
   No confirmed solution means no synthetic fire.
8. Give local tee Laser and motion that will cross freeze with a possible bounce
   afterward. Enable Self. Check preemptive confirmed rescue; already frozen,
   Old Laser, no ammo/reload or no reflection must produce no automatic shot.
9. Combine Hook Assist, macros, Freeze Avoid, weapon aim and rescue. Open chat,
   menu/console, lose window focus, enter spectator/demo, swap dummy and press
   F12. Verify immediate ownership release and no late/duplicate fire edges.
10. Restart five times after changing all profiles and rescue/planner settings.
    Compare saved values, existing appearance/effects/macros and Debug/Release
    behavior. Record planner average/worst/candidate/cache figures on real maps
    and compare frame time against the same branch with features off.

## Changed files

Build/CI: `CMakeLists.txt`, `.github/workflows/prism-windows.yml`.
Validation/report: `scripts/smoke_prism.py`, `src/test/prism_test.cpp`,
`src/test/prism_physics_test.cpp`, `docs/INTENT_PATHFINDER.md`.
Configuration: `src/engine/shared/prism_assist_variables.h`.
Integration/UI: `src/game/client/components/controls.cpp`,
`src/game/client/components/menus_settings_prism.cpp`,
`src/game/client/gameclient.h`, `src/game/client/gameclient.cpp`,
`src/game/client/prism_assist.h`, `src/game/client/prism_assist.cpp`.
New planner/weapon interfaces: `src/game/client/prism_route.h`,
`src/game/client/prism_weapon.h`, `src/game/client/prism_weapon.cpp`,
`src/game/client/prism_shot.h`.
Prediction support: `src/game/client/prediction/gameworld.h`,
`src/game/client/prediction/gameworld.cpp`,
`src/game/client/prediction/entities/character.h`,
`src/game/client/prediction/entities/laser.h`,
`src/game/client/prediction/entities/laser.cpp`.
