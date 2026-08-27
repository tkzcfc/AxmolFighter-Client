# 施法对象图（Skill / SkillManager）

施法状态在 `Object` 图上：`SkillManager` 持有 `Skill` 列表和当前/预输入会话。`SkillCastComponent` 只持有 manager。技能树拓扑仍由 `SkillTreeBuilder` 按卡组建，不在这里再建第二套树。

## 目录

| 路径 | 作用 |
|------|------|
| `Skill.*` | 单条技能：CD、次数、消耗缩放 |
| `SkillManager.*` | 选技、preset、取消窗、跑取消、管道/朝向、输入缓冲 |
| `../component/SkillCastComponent.*` | 快照容器：manager + pending blob |
| `../component/SkillDeckComponent.*` | 卡组拓扑（id / 槽下标）；不含 CD |
| `../system/BehaviorTreeSystem.*` | ensure → bindConfig → restore → `manager->update` |
| `../debug/WorldHash.*` | 混入会话标量 + 各 Skill 的 CD/次数 |

## 对象图

- `Skill`：`bindFromConfig` / `tick`。CD 到 0 回一段次数，未满则再开下一轮。
- `SkillManager`：按 `SkillDeckComponent` 重建 `Skill` 列表（拓扑以卡组为准）。`presetSkill` / `isAllowCast` / `castBegan`（costPaid 幂等）/ `castEnded`（只清朝向）/ 取消窗判定。爆气按 EP 每毫秒衰减结束；爆气中跳过 MP/EP。不建树。
- 不设 `SkillFactory`（类型只有两种）。

## 快照

1. `bindConfig` 按卡组重建 `Skill` 列表。
2. `restoreRuntimeData()` 按 `typeName` + 标量填回已 bind 的列表（个数或 id 对不上则断言）。
3. 当前技能、pipe/toward、`costPaid`、各技能 `coolDownMs`/`releaseCount` 活在对象上，进出快照后续得上且不重扣。

## 与行为树

`SkillTreeBuilder` 仍按卡组建 Slot→Toward。系统/叶子只调 `SkillManager::of(entity)`，不读组件施法裸字段。系统顺序：`AISystem` 在 `BehaviorTreeSystem` 之前；manager 在 AI 之后、树 tick 前后推进 CD（与原先时机一致）。
