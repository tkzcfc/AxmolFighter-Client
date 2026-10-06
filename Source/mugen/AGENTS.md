# mugen (shared battle core)

Compiled into **both** the client and `AxmolFighter-Server/battle`, where it is built with `RUNTIME_IN_AXMOL=0`. Any change here affects both, so check that both still build.

## Architecture
- The world is in `GameWord.cpp` (the file name is spelled "Word"). Entities are spawned in `ActorSpawner.cpp`. Components and systems are declared in `Components.h` and `Systems.h`.
- Systems run in a fixed order: Physics, then AI, then Combat, then BehaviorTree, then Displacement.
- Actions run **only** through `BehaviorTreeSystem`. The trees are rebuilt by `bt/RoleTreeBuilder`, `SkillTreeBuilder` and `EffectTreeBuilder`. Only node state is serialized, in DFS order and validated by `typeName`. Leaf actions live in `bt/actions/`.
- Keep Pool/Manager objects for features with lists, events, cooldowns or pausing. Don't force them into ECS form. Each effect instance is one `Effect` object; there is no EffectManager.
- `avatar/` must not include axmol or spine headers. Rendering code goes in `render/` inside `#ifdef RUNTIME_IN_AXMOL`.
- Avatar has no layers: one `SpineBody` plus `AvatarAccessory` parts, driven by absolute time from one `.motion` (see README "Avatar 模型"). Only `kAsyncExclusive` avatars may change atlases or graft skins; spine data ownership is fixed at creation (`MgSkeletonData::isExclusive`), never inferred from `use_count`.
- Game events come only from the logic-side `.box` data (`MotionPlayer::step`), never from rendering.
- Config: `conf/TableConfig.h` and `conf/Config.*` load `config.bin`. Struct fields must match the Lua tables one to one.

## Object class layout (in this order)
1. `typedef Super`
2. ctor/dtor and public methods
3. private methods
4. private `m_` members
5. public members (no `m_` prefix)
6. The serialization macro:
   - Use `MG_DEFINE_SERIALIZABLE(...)`.
   - For `unique_ptr` lists, use `MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl, ...)`. The impl functions go in a trailing `private` section.

Use `MG_GET_COMPONENT` to get components. Its result can be null; nothing else needs a null check.
