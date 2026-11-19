# mugen (shared battle core)

Compiled into the client and into the battle server. The server build defines `RUNTIME_IN_AXMOL=0`. Logic changes affect both, so keep the headless build green.

## Layers

```
Source/mugen/
  core/      namespace mugen. No engine dependency.
    math/    Fixed (Q44.20), FixedVec2/3, FixedMath, Random, DamageBox
    reflect/ MG_REFLECT, type traits, visitor protocol
    serialize/ ByteBuffer, BinaryArchive, Hash
    ecs/     ECSManager, entity handles, ComponentPool, singletons (data ECS; no System.h)
    io/ utils/ StdC.h MacroDefinition.h。io 只持有进程入口 set 的 VFS 指针（实现在 Source/3rd/vfs）；Content 挂在虚根 `/`，资源路径为 /mugen/...
  conf/      namespace mugen::conf. Reflected config tables and runtime Config.
             config.bin packs 34 shared tables plus 10 conf::battle derived tables
             (SkillDef, ClipDef, EffectClipDef, EffectDef, HitDef, RoleDef, BuffDef, AbilityDef, AiDef, SkillAiDef).
             12 source-only battle tables live in AxmolFighter-Tools/config_converter/source/ (MG_SOURCE_TABLES).
  avatar/    MotionPlayer and avatar/data (.motion, .box, avatar.bin). No engine headers.
  render/    Client only. The whole tree is inside #ifdef RUNTIME_IN_AXMOL. Battle presenters live in render/battle/ (one class per .h/.cpp). Roles attach under the map entity layer.
  sim/       namespace mugen::sim. Sole ECS battle and town simulation (DESIGN.md, slices S0–S8 all completed). kWorldSnapshotVersion = 9; .mgrp replay v5.
```

Headless targets (`AxmolFighter-Client/TestsHeadless`): `mugen_sim_tests` (354 unit tests covering core math/reflect/ECS, config/avatar, sim systems, buffs, skills, AI, rooms, combo, presentation/views/town, rollback/lockstep/net sync) and `mugen_sim_cli` (`run-summary`, `compare-summary`, `compare-scenario`, `run-ai-room`). Golden regression baselines in `TestsHeadless/data/golden_summaries/` are compared with `mugen_sim_cli compare-scenario --all` (run from `TestsHeadless`; format in DESIGN.md appendix A).

`render/` and `Source/ui/` are the only places that may include the engine. `avatar/` must not include engine or spine headers.

## Simulation (`sim/`)

- `kSchedule` in `sim/Schedule.cpp` is the only frame order (phases P0..P12, see `Schedule.h`). Each system is `void runXxx(World&)` in `<domain>/XxxSystem.cpp`, declared in `Systems.h`.
- Every system file starts with a four-line header: `// [row] Name`, `// 读：…`, `// 写：…`, `// 消费：…`, `// 产生：…` (reads / writes / consumes / produces), matching the DESIGN §3.2 row.
- Systems never include or call each other. They communicate only through components and the `EventQueue<T>` singletons registered in `Singletons.h`. Requests (`XxxRequest`) have one draining owner system; facts (`XxxEvent`) are read-only and cleared by `FlushSystem`. All event structs live in `Events.h`.
- Stateless rule libraries live in `<domain>/XxxRules.{h,cpp}` under `mugen::sim::<domain>::rules` (e.g. `skill::rules::beginCast`, `buff::rules::admit/remove`, `displacement::rules::set`). They write only their own domain's components and may append events; they are the only sanctioned synchronous chain (DESIGN P-1).
- `MG_COMPONENT_LIST` in `sim/Components.h` and `MG_SINGLETON_LIST` in `sim/Singletons.h` are the only registries; their order fixes snapshot type ids. Components hold gameplay state only. Summary / trace bookkeeping (trace lines, pending `disp-` endpoints) lives on `World` as non-serialized, non-hashed members.
- Buff engine lives in `sim/buff/` (DESIGN §5): `BuffComponents.h` (`BuffListComponent`/`AbilityListComponent`/`RageStateComponent`/`ComboMeterComponent`), `BuffRules.h/.cpp` (`admit`/`remove`/`fire`/`tick`), `BuffEval.cpp` (`ValueExpr`/`Predicate`), `BuffOps.cpp` (op interpreter), `BuffInternal.h` (exec state), `BuffTickSystem.cpp` (P2). Buff interval/times/duration, add-CD, and special-ability CD/period continue during hit-stop (DESIGN §3.4). Clip buffs are still admitted on clip enter and removed on clip exit.
- Data flow: source tables `buff`/`buff_rule`/`special_ability` → `config_converter/lower/` (`BuffLower`/`AbilityLower`/`LowerBuffShared`) → `conf/battle::{BuffDef,AbilityDef}` → `config.bin` → runtime interpreter (op / predicate / `ValueExpr`). Lowering (`Content/mugen/config/lower_report.txt`): buff 9337 rows (9203 lowered, 128 no-op, 6 unsupported); ability 854 rows (308 lowered, 546 no-op). No-op scope is artifact, crystal, and controllable-effect ops.
- Runtime battle tables are `conf::battle::*Def` (derived tables lowered by the converter; do not edit by hand). UI/rendering reads those defs directly (SkillDef `icon`, RoleDef `headImage`/`resFashion`/`tier`/`spineRelativePosition`, EffectDef `tier`/`spineRelativePosition`, BuffDef `presentation`).
- Golden equivalence: `mugen_sim_cli compare-scenario --all` (run from `TestsHeadless`) against `data/golden_summaries/`. Never edit a golden to make a scenario pass; list differences as proposals in a report. Buff goldens are `data/scenarios/buff_*.script` with coverage map `data/golden_summaries/buff_coverage.txt`. Scripts may inject `frame buff <target> <buffId>` (DESIGN A.6; not written into `FrameInput`) and `frame cmd <revive|return|warndone>` (DESIGN A.7). Manifest accepts an optional 8th `copy` column.
- Monster AI (`sim/ai/`): `AiComponents.h`/`AiRules.h/.cpp`/`AiSystem.cpp` — rule table → `IntentComponent`, same cast/move path as players. `mugen_sim_cli run-ai-room <roomId> --frames N [--class 1-7] [--seed N] [--level N] [--stall N]` runs a full level with monster AI on.
- Rooms (`sim/room/`): `RoomComponents.h` obstacle/goods/portal/drop-table, `RoomRules.h/.cpp`, `RoomDirectorSystem.cpp`/`ObstacleSystem.cpp`/`InteractSystem.cpp`/`RoomTransferSystem.cpp` — PvE room state machine (waves, drops, portals, revive, KO) and PvP duel director.
- Presentation & read-only views (`sim/present/PresentationEvent.h`, `sim/present/PresentationSystem.cpp`, `sim/view/{WorldView,HudView,DebugView}.{h,cpp}`, `sim/input/ControlManagerCore.{h,cpp}`, `sim/replay/Replay.{h,cpp}` v5): `PresentationSystem` (P11.1) converts fact queues into `PresentationEvents` without touching `RngState`. `render/` and `Source/ui/` obey DESIGN P-7 (enforced by `check_rules.ps1`): they only include `World.h`, `WorldSetup.h`, `BattleBootstrap.h`, `present/PresentationEvent.h`, `view/*`, `input/*`, `net/*`, `rollback/*`, `replay/*`, never component headers.
- Networking & rollback (`sim/rollback/RollbackSession.{h,cpp}`, `sim/net/{NetCodec,LockstepAuthority,NetBattleSync}.{h,cpp}`): lockstep authority, adaptive-lead rollback session, and presentation event deduplication. `WorldSetup`/`PlayerSetup` are public structs with bare `camelCase` fields (no `m_class`; occupation is derived from `roleId`).
- Client (`LocalBattleMode`, `NetBattleMode`, `TownScreen`) and battle server (`AxmolFighter-Server/battle`) all compile and link `Source/mugen/sim/`.

## Prohibitions

In `core/` and `sim/`:

- Do not include engine, spine, imgui, or rapidjson headers.
- Do not use `float` or `double`. The exception is a function whose name says it converts for rendering.
- Do not call `rand()`, read the system clock, or order by pointer address.
- Do not put `std::unordered_map` or `std::unordered_set` in state that is serialized or that controls traversal order.
- Do not use `std::function`. Callbacks are function-pointer tables or `enum` plus `switch`.

Comments may be Chinese. Log, error, and assert strings are English. Comments must not name the reference product and must not contain a drive-letter path or a plan-document path.

## Binding code conventions

- C++ `class` non-public data members use the `m_` prefix; `struct` fields use bare `camelCase` names without `m_`. Restrict `struct` to public data aggregates (ECS components/singletons/events/views, config/IO records, value types, with at most lightweight helpers); types encapsulating private state, invariants, or resource lifecycles must be `class`.
- `MG_COMPONENT_LIST`, `MG_SINGLETON_LIST`, `MG_CONFIG_TABLES`, and `MG_DERIVED_TABLES` keep one entry per line inside `// clang-format off`.
- Comment every type, member variable, and function with a concise Chinese comment. Trivial accessors may omit one.
- In `core/` and `sim/`, do not use runtime exceptions for error handling. Use English `MG_ASSERT` plus `MG_LOG_E`; return `bool`, `std::optional`, or `nullptr` only when a caller must gracefully handle failure.
- Put readability first: use plain, explicit constructs; do not introduce clever templates or surprising implicit conversions.
- Mugen may run multiple instances in one server process: only process-wide read-only data may be singleton or static; per-battle mutable state must be owned by an instance.

No compatibility shim. Interfaces can change. Do not null-check a value the type system already proves. The only nullable lookups are `ECSManager::tryGet<T>()` and `Config::find<T>()`.

## ECS

- `Entity` is a generational handle. Index 20 bits, generation 12 bits, `0` is invalid. Freed slots recycle FIFO.
- Components are copyable reflected structs. No virtual functions, no owning pointers.
- `MG_COMPONENT_LIST` in `sim/Components.h` is the only component registry. `MG_SINGLETON_LIST` in `sim/Singletons.h` is the only singleton registry.
- `each<Ts...>()` walks in creation order. The upper bound is fixed when the walk starts, so entities created during the walk are not visited in the same frame.
- `destroy()` marks. `flushDestroyed()` recycles. `serialize`, `deserialize`, `clone`, and `hash` round-trip the manager.

## Monster AI

- Monster decisions are the constexpr rule table `kMonsterRules` in `sim/ai/AiRules.h`. Each row has up to three tests, one action, an optional alert flag and an optional cooldown. Add behaviour by adding a row, an `AiCondition` or an `AiAction`; do not add a tree.
- `AiSystem` selects the target, builds read-only facts, and writes `IntentComponent`. No decision is made during an attack. While a move runs, only a higher-priority row may preempt it.
- Actions share the player cast/move path (`RoleFsmSystem` / `LocomotionSystem`).
- `ChancePercent` must be the last test of its row, so failed tests never consume the random stream.
- `WorldSetup::aiEnabled` turns monster decisions off. It is archived with the setup so replay stays consistent.

## Config

- `MG_CONFIG_TABLES` in `conf/ConfigTables.h` is the runtime table registry (shared tables + 10 `MG_DERIVED_TABLES` in `conf/battle/`). `MG_CONFIG_REFS` declares cross-table references among packed tables.
- 12 source-only battle tables (`skill`, `skill_hit`, `skill_ai`, `skill_change_system`, `action`, `action_effect`, `role`, `effect`, `ai`, `buff`, `buff_rule`, `special_ability`) are registered in `AxmolFighter-Tools/config_converter/source/SourceTables.h` (`MG_SOURCE_TABLES`, plus `SourceRefs.h` and `BuffRules.h`). Pack reads them for reference checking and lowering; they are not serialized into `config.bin`.
- A C++ struct is the schema. The packer matches Lua fields both ways and reports the path of every mismatch.
- `config.bin` starts with magic `MGCF`, a version, and a schema hash. A hash mismatch refuses to load. Derived `conf/battle::*Def` are packed into the same archive and participate in `schemaHash`.
- `Config::find<T>()` may return null. `Config::get<T>()` asserts and logs.

## Simulation step

- `sim::World::tick(const FrameInput&)` is the only frame step. Order is `kSchedule` (P0..P12).
- Position is `(x, y, z)`: horizontal, height, depth. Facing is `towardX` in `{-1, 1}`.
- Role state is a single `RoleStateId` plus derived status bits. Components may have non-virtual member functions over their own data; no owning pointers, no virtuals.

## Hit volumes

Combat overlap is a 3-axis world box. It is not a screen rectangle plus a separate radius test. Rules live in `sim/anim/HitboxRules.h`.

- World-box axes: X horizontal, Y depth (along transform Z), Z height (along transform Y). Height is `position.y + localZ * scale`. Do not fold `position.y + position.z` onto one vertical axis.
- Width and height come from the current `.box` rectangle, multiplied by spine scale, and mirrored on X when `towardX == -1`.
- `RoleDef.depth` / `EffectDef.depth` are derived while lowering config: `radius > 0 ? radius * 2 : 0`. There is no depth column in the lua tables. Runtime still uses this value and ignores the file depth.
- `.box` export writes a preview of that override so the editor matches the game shape. Local `size.y = round(2 * radius / spineScale)` and `pos.y = -size.y / 2`, because the editor draws the skeleton unscaled while the game scales width and height but not the overridden depth. The depth comes from a role that uses the skeleton, or from an effect when no role does. Several radii pick the most common depth, then the smallest id. A skeleton with no positive radius keeps `size.y = 10` and `pos.y = 0`.
- `depth > 0`: the depth interval is `[position.z - depth/2, position.z + depth/2)`, full length, unscaled, shared by every frame of that entity. `depth == 0`: use that frame's `.box` `size.y * scale`, origin `position.z + pos.y * scale`.
- A hit requires a positive-length overlap on all three axes (half-open: `start < end`). A zero-length interval never overlaps. Radius does not gate hits.
- Radius stays on the entity for movement, wall probes, obstacle depth push, and AI spacing.
- Contact is in Transform coordinates: x is the horizontal center of the overlap, y is the height center (box Z), z is the depth center (box Y). The hit spark is spawned at `{contact.x, contact.y, contact.z - 1}`. Hurt numbers stay on the screen: `{contact.x, contact.y + contact.z}` plus `hurtNumPos`.
- Obstacles, portals, and goods have no depth field. Their volume is a radius on X and depth, and a diameter of height starting at `position.y`.
- Debug boxes use the editor's oblique projection, sheared around the entity's own depth, and are drawn at the interpolated display offset (`PoseTrack::displayOffset`). Logic collision still uses the uninterpolated pose.
- Cross-entity references are `Entity` handles.
- The sim writes presentation events. It never calls rendering or audio. The presentation layer never writes sim state.
- One `Random` lives in the `RngState` singleton. Logic code does not construct another one.

## Tests

From the client repo:

```
pwsh TestsHeadless/run_headless_tests.ps1 [-Filter <doctest filter>] [-Config Release|Debug]
pwsh TestsHeadless/check_rules.ps1 [-Paths core,conf,avatar,sim]
```

`run_headless_tests.ps1` configures, builds, runs `mugen_sim_tests`, and returns its exit code. `check_rules.ps1` prints `file:line: rule` and returns non-zero on a violation. `mugen_sim_cli` commands: `run-summary`, `compare-summary`, `compare-scenario`, `run-ai-room`. Unit tests live in `TestsHeadless/sim_tests/` (core math/reflect/ECS, config/avatar, sim systems, buffs, skills, AI, rooms, combo, presentation/views/town, rollback/lockstep/net; 350 cases).

## VFS mount topology (client)

Headless tests and the client share keys under `/mugen/...` (absolute, or relative `mugen/...` which `VfsFileUtils` prefixes with `/` for VFS). Shared code uses `io::vfs()`.

| Mount | Role |
|---|---|
| `/` | Read-only Content (native dir on desktop; `content.vfs` archive if present on Android, else AssetManager) |
| `/user` | Writable native directory (`WriteTarget`) |

`io::setVfs` must run before `Config::load` / `AvatarAssetCache::load`. `ScenarioHarness` mounts both `/` and `/user`. Do not reference `res_zhcn`.
On Android, `content.vfs` holds most Content when packed; without it the AssetManager is still mounted at `/` so VFS paths work. `Content/native/` (videos under `native/video/`) is loose under `assets/native` (MediaPlayer needs native paths; game code keeps `video/...` via native search path). Audio can stay in the `.vfs`. Gradle: `CONTENT_VFS_PACK` in `proj.android/gradle.properties`.
