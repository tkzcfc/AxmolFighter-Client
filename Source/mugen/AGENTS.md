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
    io/ utils/ StdC.h MacroDefinition.h
  conf/      namespace mugen::conf. Reflected config tables and runtime Config.
  avatar/    MotionPlayer and avatar/data (.motion, .box, avatar.bin). No engine headers.
  render/    Client only. The whole tree is inside #ifdef RUNTIME_IN_AXMOL.
  sim/       namespace mugen::sim. Battle logic. No engine dependency.
```

`render/` and `Source/ui/` are the only places that may include the engine. `avatar/` must not include engine or spine headers.

## Prohibitions

In `core/` and `sim/`:

- Do not include engine, spine, imgui, or rapidjson headers.
- Do not use `float` or `double`. The exception is a function whose name says it converts for rendering.
- Do not call `rand()`, read the system clock, or order by pointer address.
- Do not put `std::unordered_map` or `std::unordered_set` in state that is serialized or that controls traversal order.
- Do not use `std::function`. Callbacks are function-pointer tables or `enum` plus `switch`.

Comments may be Chinese. Log, error, and assert strings are English. Comments must not name the reference product and must not contain a drive-letter path or a plan-document path.

No compatibility shim. Interfaces can change. Do not null-check a value the type system already proves. The only nullable lookups are `ECSManager::tryGet<T>()` and `Config::find<T>()`.

## ECS

- `Entity` is a generational handle. Index 20 bits, generation 12 bits, `0` is invalid. Freed slots recycle FIFO.
- Components are copyable reflected structs. No virtual functions, no owning pointers.
- `MG_COMPONENT_LIST` in `sim/Components.h` is the only component registry. `MG_SINGLETON_LIST` in `sim/Singletons.h` is the only singleton registry.
- `each<Ts...>()` walks in creation order. The upper bound is fixed when the walk starts, so entities created during the walk are not visited in the same frame.
- `destroy()` marks. `flushDestroyed()` recycles. `serialize`, `deserialize`, `clone`, and `hash` round-trip the manager.

## Behavior tree

- `BTDef` is immutable and shared. `BTInstance` is a value stored on a component. Snapshots store the definition key plus the instance, not the topology.
- Composites are Selector, Sequence, and Parallel. A guard failure makes the composite return Success. A Selector does not preempt a running child. The root does not restart itself.
- Leaves and conditions register function pointers on `BTRegistry`. An unported leaf is `kUnimplementedLeaf`. Nothing else is a placeholder.

## Config

- `MG_CONFIG_TABLES` in `conf/ConfigTables.h` is the only table registry. `MG_CONFIG_REFS` declares cross-table references.
- A C++ struct is the schema. The packer matches Lua fields both ways and reports the path of every mismatch.
- `config.bin` starts with magic `MGCF`, a version, and a schema hash. A hash mismatch refuses to load.
- `Config::find<T>()` may return null. `Config::get<T>()` asserts and logs.

## Simulation

- `sim::World::tick(const FrameInput&)` is the only frame step. Order: clock, logic timers, control events, camera and slow motion, entity groups (Role, Effect, Obstacle, Goods, Portal), scene, collision, `flushDestroyed`.
- Position is `(x, y, z)`: horizontal, height, depth. Facing is `towardX` in `{-1, 1}`.
- Status bits are the only character state. Pool data (skills, buffs, abilities) is component data plus free functions, not pointer-owning objects.
- Cross-entity references are `Entity` handles.
- The sim writes presentation events. It never calls rendering or audio. The presentation layer never writes sim state.
- One `Random` lives in the `RngState` singleton. Logic code does not construct another one.

## Tests

From the client repo:

```
pwsh TestsHeadless/run_headless_tests.ps1 [-Filter <doctest filter>] [-Config Release|Debug]
pwsh TestsHeadless/check_rules.ps1 [-Paths core,conf,avatar,sim]
```

`run_headless_tests.ps1` configures, builds, runs `mugen_tests`, and returns its exit code. `check_rules.ps1` prints `file:line: rule` and returns non-zero on a violation. `mugen_sim_cli` replays and runs rooms headless.
