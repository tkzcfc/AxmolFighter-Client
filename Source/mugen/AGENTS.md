# mugen (shared battle core)

Compiled into the client and into the battle server. The server build defines `RUNTIME_IN_AXMOL=0`. Logic changes affect both, so keep the headless build green.

## Layers

```
Source/mugen/
  core/      namespace mugen. No engine dependency.
    math/    Fixed (Q44.20), FixedVec2/3, FixedMath, Random, DamageBox
    reflect/ MG_REFLECT, type traits, visitor protocol
    serialize/ ByteBuffer, BinaryArchive, Hash
    ecs/     ECSManager, entity handles, ComponentPool, singletons
    bt/      BTDef / BTInstance / BTRunner / BTRegistry
    io/ utils/ StdC.h MacroDefinition.h。io 只持有进程入口 set 的 VFS 指针（实现在 Source/3rd/vfs）；Content 挂在虚根 `/`，资源路径为 /mugen/...
  conf/      namespace mugen::conf. Reflected config tables and runtime Config.
  avatar/    MotionPlayer and avatar/data (.motion, .box, avatar.bin). No engine headers.
  render/    Client only. The whole tree is inside #ifdef RUNTIME_IN_AXMOL.
  sim_legacy/ namespace mugen::sim_legacy. Current battle logic, frozen (no new features; the new ECS sim goes into a new `sim/` per DESIGN.md). No engine dependency.
  sim/       namespace mugen::sim. New ECS battle sim (DESIGN.md, slices S0–S8; S0 + S1 + S2 done). See "New sim (sim/)" below. Never include sim_legacy.
```

Headless targets: `mugen_tests` and `mugen_legacy_cli` link `sim_legacy`; `mugen_sim_tests` and `mugen_sim_cli` link `sim`. No binary links both. Equivalence is checked with effect-summary goldens in `TestsHeadless/data/golden_summaries/` (`mugen_legacy_cli export-summary`, `compare-summary`, `run_summary_export.ps1`; format in DESIGN.md appendix A).

`render/` and `Source/ui/` are the only places that may include the engine. `avatar/` must not include engine or spine headers.

## New sim (`sim/`)

- `kSchedule` in `sim/Schedule.cpp` is the only frame order (phases P0..P12, see `Schedule.h`). Each system is `void runXxx(World&)` in `<domain>/XxxSystem.cpp`, declared in `Systems.h`.
- Every system file starts with a four-line header: `// [row] Name`, `// 读：…`, `// 写：…`, `// 消费：…`, `// 产生：…` (reads / writes / consumes / produces), matching the DESIGN §3.2 row.
- Systems never include or call each other. They communicate only through components and the `EventQueue<T>` singletons registered in `Singletons.h`. Requests (`XxxRequest`) have one draining owner system; facts (`XxxEvent`) are read-only and cleared by `FlushSystem`. All event structs live in `Events.h`.
- Stateless rule libraries live in `<domain>/XxxRules.{h,cpp}` under `mugen::sim::<domain>::rules` (e.g. `skill::rules::beginCast`, `buff::rules::admit/remove`, `displacement::rules::set`). They write only their own domain's components and may append events; they are the only sanctioned synchronous chain (DESIGN P-1).
- `MG_COMPONENT_LIST` in `sim/Components.h` and `MG_SINGLETON_LIST` in `sim/Singletons.h` are the only registries; their order fixes snapshot type ids. Components hold gameplay state only. Summary / trace bookkeeping (trace lines, pending `disp-` endpoints) lives on `World` as non-serialized, non-hashed members.
- Buff engine lives in `sim/buff/` (DESIGN §5, slice S2), op-based rather than per-rule classes: `BuffComponents.h` (`BuffList`/`AbilityList`), `BuffRules.h/.cpp` (`admit`/`remove`/`fire`/`tick`), `BuffEval.cpp` (`ValueExpr`/`Predicate`), `BuffOps.cpp` (op interpreter), `BuffInternal.h` (exec state), `BuffTickSystem.cpp` (P2). Buff interval/times/duration, add-CD, and special-ability CD/period continue during hit-stop (DESIGN §3.4). Clip buffs are still admitted on clip enter and removed on clip exit.
- Data flow: source tables `buff`/`buff_rule`/`special_ability` → `config_converter/lower/` (`BuffLower`/`AbilityLower`/`LowerBuffShared`) → `conf/battle::{BuffDef,AbilityDef}` → `config.bin` → runtime interpreter (op / predicate / `ValueExpr`). Lowering (`Content/mugen/config/lower_report.txt`): buff 9337 rows (9203 lowered, 128 no-op, 6 unsupported); ability 854 rows (308 lowered, 546 no-op). No-op scope is artifact, crystal, and controllable-effect ops.
- Runtime battle tables are `conf::battle::*Def` (derived tables lowered by the converter; do not edit by hand).
- Golden equivalence: `mugen_sim_cli compare-scenario --all` (run from `TestsHeadless`) against `data/golden_summaries/`. Never edit a golden to make a scenario pass; list differences as proposals in a report. S2 buff goldens are `data/scenarios/buff_*.script` with coverage map `data/golden_summaries/buff_coverage.txt`. Scripts may inject `frame buff <target> <buffId>` (DESIGN A.6; not written into `FrameInput`). `buff_spawn` currently fails; that is S3 hit-state work, not an S2 gap.

## Prohibitions

In `core/`, `sim/` and `sim_legacy/`:

- Do not include engine, spine, imgui, or rapidjson headers.
- Do not use `float` or `double`. The exception is a function whose name says it converts for rendering.
- Do not call `rand()`, read the system clock, or order by pointer address.
- Do not put `std::unordered_map` or `std::unordered_set` in state that is serialized or that controls traversal order.
- Do not use `std::function`. Callbacks are function-pointer tables or `enum` plus `switch`.

Comments may be Chinese. Log, error, and assert strings are English. Comments must not name the reference product and must not contain a drive-letter path or a plan-document path.

## Binding code conventions

- Comment every type, member variable, and function with a concise Chinese comment. Trivial accessors may omit one.
- In `core/` and `sim_legacy/`, do not use runtime exceptions for error handling. Use English `MG_ASSERT` plus `MG_LOG_E`; return `bool`, `std::optional`, or `nullptr` only when a caller must gracefully handle failure.
- Put readability first: use plain, explicit constructs; do not introduce clever templates or surprising implicit conversions.
- Mugen may run multiple instances in one server process: only process-wide read-only data may be singleton or static; per-battle mutable state must be owned by an instance.

No compatibility shim. Interfaces can change. Do not null-check a value the type system already proves. The only nullable lookups are `ECSManager::tryGet<T>()` and `Config::find<T>()`.

## ECS

- `Entity` is a generational handle. Index 20 bits, generation 12 bits, `0` is invalid. Freed slots recycle FIFO.
- Components are copyable reflected structs. No virtual functions, no owning pointers.
- `MG_COMPONENT_LIST` in `sim_legacy/Components.h` is the only component registry. `MG_SINGLETON_LIST` in `sim_legacy/Singletons.h` is the only singleton registry.
- `each<Ts...>()` walks in creation order. The upper bound is fixed when the walk starts, so entities created during the walk are not visited in the same frame.
- `destroy()` marks. `flushDestroyed()` recycles. `serialize`, `deserialize`, `clone`, and `hash` round-trip the manager.

## Behavior tree

- Behavior trees now only drive effects; role brains are driven by the explicit state machine in `sim_legacy/role/brain/`.
- `BTDef` is immutable and shared. `BTInstance` is a value stored on a component. Snapshots store the definition key plus the instance, not the topology.
- Composites are Selector, Sequence, and Parallel. A guard failure makes the composite return Success. A Selector does not preempt a running child. The root does not restart itself.
- Leaves and conditions register function pointers on `BTRegistry`. An unported leaf is `kUnimplementedLeaf`. Nothing else is a placeholder.

## Monster AI

- Monster decisions are the constexpr rule table `kMonsterRules` in `sim_legacy/ai/AiRules.h`. Each row has up to three tests, one action, an optional alert flag and an optional cooldown. Add behaviour by adding a row, an `AiCondition` or an `AiAction`; do not add a tree.
- `AiDriver::update` selects the target in code, builds a read-only `AiFacts` with `gather()`, and runs the first matching row from `pickRule`. No decision is made during an attack. While a move runs, only a higher-priority row may preempt it.
- Actions only start a status through `Role::changeStatus`. The role state machine (`CastState`, `AiMove`) executes them.
- `ChancePercent` must be the last test of its row, so failed tests never consume the random stream.
- `WorldSetup::m_aiEnabled` (`mugen_legacy_cli --ai off`) turns monster decisions off. It is not part of the archived setup.

## Config

- `MG_CONFIG_TABLES` in `conf/ConfigTables.h` is the only table registry. `MG_CONFIG_REFS` declares cross-table references.
- A C++ struct is the schema. The packer matches Lua fields both ways and reports the path of every mismatch.
- `config.bin` starts with magic `MGCF`, a version, and a schema hash. A hash mismatch refuses to load. Derived `conf/battle::*Def` (including `BuffDef`/`AbilityDef`) are packed into the same archive and participate in `schemaHash`.
- `Config::find<T>()` may return null. `Config::get<T>()` asserts and logs.

## Simulation

- `sim_legacy::World::tick(const FrameInput&)` is the only frame step. Order: clock, logic timers, control events, camera and slow motion, entity groups (Role, Effect, Obstacle, Goods, Portal), scene, collision, `flushDestroyed`.
- Position is `(x, y, z)`: horizontal, height, depth. Facing is `towardX` in `{-1, 1}`.
- Status bits are the only character state. Components may have non-virtual member functions over their own data; temporary view classes are allowed; no owning pointers, no virtuals.
- Cross-entity references are `Entity` handles.
- The sim writes presentation events. It never calls rendering or audio. The presentation layer never writes sim state.
- One `Random` lives in the `RngState` singleton. Logic code does not construct another one.

## Tests

From the client repo:

```
pwsh TestsHeadless/run_headless_tests.ps1 [-Filter <doctest filter>] [-Config Release|Debug]
pwsh TestsHeadless/check_rules.ps1 [-Paths core,conf,avatar,sim_legacy]
```

`run_headless_tests.ps1` configures, builds, runs `mugen_tests`, and returns its exit code. `check_rules.ps1` prints `file:line: rule` and returns non-zero on a violation. `mugen_legacy_cli` replays and runs rooms headless. `mugen_sim_tests` Buff unit tests: `TestsHeadless/sim_tests/BuffLifecycleTest.cpp`, `BuffOpsTest.cpp`, `BuffPredicateTest.cpp`, `BuffRulesRefTest.cpp`.


## VFS mount topology (client)

Headless tests and the client share keys under `/mugen/...` (absolute, or relative `mugen/...` which `VfsFileUtils` prefixes with `/` for VFS). Shared code uses `io::vfs()`.

| Mount | Role |
|---|---|
| `/` | Read-only Content (native dir on desktop; `content.vfs` archive if present on Android, else AssetManager) |
| `/user` | Writable native directory (`WriteTarget`) |

`io::setVfs` must run before `Config::load` / `AvatarAssetCache::load`. `ScenarioHarness` mounts both `/` and `/user`. Do not reference `res_zhcn`.
On Android, `content.vfs` holds most Content when packed; without it the AssetManager is still mounted at `/` so VFS paths work. `Content/native/` (videos under `native/video/`) is loose under `assets/native` (MediaPlayer needs native paths; game code keeps `video/...` via native search path). Audio can stay in the `.vfs`. Gradle: `CONTENT_VFS_PACK` in `proj.android/gradle.properties`.
