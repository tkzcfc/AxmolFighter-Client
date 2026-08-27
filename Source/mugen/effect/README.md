# 特效对象图（Effect）

特效是独立实体上的一个 `Effect`，不是角色 Manager 下的列表。`EffectComponent` 只持有该对象。命中会话、寿命、弹道活在 `Effect` 上。

## 目录

| 路径 | 作用 |
|------|------|
| `Effect.*` | 单特效：跟随/弹道/寿命、命中会话、`tryHit` |
| `../component/EffectComponent.*` | 快照容器：Effect + pending blob |
| `../system/EffectLifeSystem.*` | spawn / ensure → restore → `effect->update` |
| `../combat/CombatHit.*` | 敌我、盒、`applyHit`（近战与 tryHit 共用） |
| `../bt/EffectTreeBuilder.*` | 按 `action_ids` 查特效动作表挂 AttackAction；无动作则 Hold |

## 对象图

- 不设 `EffectFactory`（类型一种）。
- `Effect::of(entity)`；系统/叶子不读组件玩法裸字段。
- `autoRelease` 是枚举 0/1/2/3，寿命跟动作树 / `destroyRequested`（满命中仅 `hit_interval<0`）；`follow==2` 贴地锁高度。
- 展示 Spine（Buff 跟随）走同一套 spawn，`effectId=0` 不结算命中。必杀 Spine 是屏幕覆盖，不走特效实体。

## 快照

1. spawn 时 `bindFromConfig` / `bindVisual` 写入表 id 与初始标量。
2. `restoreRuntimeData()` 按 `typeName` + 标量填回（对不上则断言）。
3. EFFECT 树由 Builder 按 `action_ids` 重建（查特效动作表，不与角色 `action_attack` 共用），再 DFS 填节点标量。命中次数只在 Effect 上，不在攻击叶上重复。
