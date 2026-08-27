# Behavior Tree（技能 / 角色 / 特效）

行为树是 `Object` 图：拓扑由 Builder 按卡组重建，快照只写节点标量（DFS）。`BehaviorTreeComponent` 只持有 root / treeKind / cityMode。

## 目录

| 路径 | 作用 |
|------|------|
| `RoleTreeBuilder.*` | 角色根树（Death→Revive→Wake→Hit*→Attack→AI→移动） |
| `SkillTreeBuilder.*` | Attack 下 Slot→SlotIndex→Mode→Step→Pipe→Toward |
| `EffectTreeBuilder.*` | 特效：`action_ids` 查特效动作表挂 AttackAction（`effectTable`），否则 Hold |
| `../effect/` | `Effect` 对象图；系统/叶子只调 `Effect::of` |
| `BTTypeRegistry.*` | 工厂注册类型名（对照 FSMFactory） |
| `actions/` | Locomo / HitKind / Death / Wake / Revive / AttackAction / AIActions |
| `conditions/` | CondStatus / CondAttack* / CondAI / CondEffect* |
| `../core/bt/` | Selector/Sequence/Action 引擎（无战斗组件依赖） |
| `../debug/WorldHash.*` | 快照哈希（树 DFS + cityMode/treeKind；AI 见 `../ai/README.md`；施法见 `../skill/README.md`） |
| `../ai/` | `AiAgent` / `SkillAi` 对象图；系统只 tick，选技仍走 `presetSkill` |
| `../skill/` | `Skill` / `SkillManager` 施法对象图；系统/叶子只调 manager |
| `../buff/` | `Buff` / `BuffManager` 对象图；系统/叶子只调 manager |

## 组件

- `BehaviorTreeComponent`：root + `treeKind`（0 角色 / 1 城镇 / 2 特效）+ `cityMode`；线格式委托 `root->serialize`
- `SkillCastComponent`：只持 `SkillManager`；会话与 CD 在 manager / Skill 上
- `HitReactComponent`：受击队列与当前位移/连段标记
- `BehaviorComponent`：状态位（含 `kTagAttackState`）、rigidity、reviveRequested

## 快照

1. Builder 按 SkillDeck 重建树。
2. `restoreRuntimeData()` 按 DFS 把 `typeName` + 标量填回已建节点（对不上则断言）。
3. Selector/Sequence 的 `currentIndex`、AttackAction 的 `elapsedMs` 等活在节点上。

## 系统顺序

`PhysicsSystem` → `AISystem` → `CombatSystem` → `BehaviorTreeSystem` → `DisplacementSystem`
