# Autonomous solo Pathfinder

The Pathfinder generates ordinary DDNet inputs from the loaded map and current
prediction world. It has no prerecorded routes. This first version supports
static solo walking, jumping, hook attachment/swing/release and nearby freeze
avoidance. It does not promise completion of arbitrary KoG maps.

## Using it

Open **Prism → Pathfinder**. Enable the module in **Assist** first to inspect the
route and prediction; select **Autopilot** to execute. Finish tiles are discovered
on both game and front layers, and global routing selects the cheapest connected
finish. For local testing select **Manual**, then **Destination at cursor**.
Settings use the existing saved Prism configuration. The existing intent-aware
Freeze Avoid and its `prism_pathfinder` setting remain separate.

Console commands (also usable with `bind`):

- `prism_solo_start`, `prism_solo_stop`
- `prism_solo_pause`, `prism_solo_resume`, `prism_solo_replan`
- `prism_solo_cursor`, `prism_solo_destination <x> <y>` (world coordinates)

F12 invokes Prism's existing emergency stop. Manual movement, jump, hook, fire or
macro activity pauses Autopilot by default, preserving that physical input in the
same snapshot. Aim motion alone does not disengage. Turn off **Manual input
pauses** only when intentional exclusive control is wanted. Assist never owns
inputs. Autonomous ownership suppresses the other movement/hook/fire/weapon
controllers. Stopping sends a release snapshot through the existing monotonic
fire composer, clearing direction, jump, hook, fire and weapon cycling.

Death, lost focus/menu/spectator, disconnect, reset, dummy switch, invalid
prediction and failed recovery release ownership. Pause requires explicit resume;
unsupported worlds remain inactive. Planner failure stops rather than replaying
an unsafe sequence.

## Layers

`src/game/client/pathfinder/` separates:

- `map`: incremental cached tile analysis, eight-tile horizontal corridor spans,
  floor/ceiling/ledge/corridor characteristics, adjacency portals and exposed
  hookable faces indexed in 256-unit buckets. Reverse Dijkstra supplies a global
  corridor guide. Geometry is a proposal, never proof of physical reachability.
- `types`: exact observed physical state and quantized dominance keys. Keys retain
  velocity, jump availability/input edges, hook position/direction/state/age,
  freeze, weapon, tune zone, abilities, movement restrictions, teleport checkpoint
  and switch state. Simulation retains exact worlds, not quantized physics.
- `simulator`: detached `CGameWorld::CopyWorld(..., true)` with normal character
  `OnDirectInput`, `OnPredictedInput` and world ticks. This uses the same DDNet
  acceleration, gravity, jumping, friction, hooking, swinging and tile prediction
  as the client. Source entities, parent links, events and inputs stay untouched.
- `search`: resumable bounded best-first physical state search, capped at 2,048
  nodes. Movement primitives sample direction, jump/air-jump edge, hook hold and
  release over 2/4/8/12 ticks. Up to six spatially ranked hook anchors are checked
  through authoritative hook rays. Actual attachment and release velocity are
  evaluated by simulation. Dominance pruning and scoring favor corridor progress,
  useful velocity and remaining maneuverability; they do not maximize freeze
  distance. Short successful prefixes allow continual progress across obstacles.
- `controller`: current-world verification of at most 32 committed ticks,
  divergence checks against predicted position/velocity/jump/hook state, resuming
  search between snapshots and safe neutral/directional/hook-hold recovery while
  searching. Candidate plans from older snapshots are re-simulated before use.
  Eight seconds without route progress or repeated failed plans stops control.

`prism_pathfinder.cpp` connects configuration, input ownership, bounded world
visualization, status HUD and optional transition logging. Disabled operation
returns immediately. Analysis runs once per map lifetime and resets on client/map
reset. Physics and map access stay on the input thread: DDNet collision and tuning
are shared by detached worlds and are not safe for unsynchronized worker use.
Search, analysis and routing yield to configurable 0.5–8 ms soft budgets once per
prediction tick. An individual world clone/tick/allocation cannot be interrupted;
this is not a hard real-time guarantee. HUD reports measured planning time; debug
reports expansions, pruning and replans. Search statistics additionally retain
simulation, death/freeze/unsupported rejection and worst-time counters.

## Safety and limitations

Freeze is checked at the character center, with normal prediction freeze state;
death uses DDNet character sampling points. Swept samples reject hazardous crossed
segments rather than penalizing nearby freeze. Low-clearance safe corridors are
allowed. This version deliberately rejects all freeze contact, including
potentially recoverable freeze and deliberate freeze crossings.

Teleport/checkpoint/hook teleport transitions are rejected: client prediction does
not provide enough authoritative destination simulation here. Speedups, tune zones,
stoppers and character abilities use normal prediction, but are not exhaustively
validated. Dynamic doors/entities and super/invincible states are rejected because
copied door ticks mutate shared collision. Team puzzles, unknown switch outcomes,
weapon boosts, jetpack/ninja/laser mechanics and coordinated other-player movement
are unsupported. A hook attaching to another player is rejected.

Geometric routing can propose physically impossible corridors. Quantization and
bounded primitive timing can discard solutions requiring finer precision or a
long setup that initially loses progress. The next bottleneck for hard KoG maps is
learning/certifying alternative region transitions with longer momentum and swing
setup, followed by isolated teleport/switch/dynamic collision simulation.

## Validation and debugging

Build `game-client`, `testrunner`, and `prism-physics-tests`; run `run_tests`.
The physics suite includes deterministic exact prediction parity/source isolation,
state equivalence, geometry/finish selection, hook line of sight, pruning/scoring,
flat traversal, death-gap jump, hook-only elevated traversal with attachment and
release, narrow freeze clearance, alternate freeze-wall routing, and closed-loop
divergence/replanning/ownership cleanup. Coordinates belong only to synthetic
fixtures; production contains no map-specific routes.

Enable **Search debug** and **Debug logging** to diagnose exhaustion, unsupported
mechanics, rejected trajectories, missed hooks and stale candidate rejection.
Rendering caps route segments, committed prediction and retained search traces;
no full explored-state cloud is drawn by default. Progress is remaining geometric
route cost, not a guarantee of eventual completion. Synthetic completion and
prediction parity do not establish complete real-map or universal KoG support.
