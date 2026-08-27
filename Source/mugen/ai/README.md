# AI（SkillAi / AiAgent）

技能 AI 是 `Object` 图：`AiAgent` 持有 `SkillAi` 列表，快照只写运行时标量。`AIComponent` 只持有 agent。巡逻/追击/警觉仍是角色树叶子，不另建一棵决策树。

## 目录

| 路径 | 作用 |
|------|------|
| `SkillAi.*` | 单条 skill_ai：次数、loadCd/checkCd、composition check |
| `AiAgent.*` | 选技、施法间隔、巡逻/警觉/移动意图 |
| `../component/AIComponent.*` | 快照容器：agent + pending blob |
| `../system/AISystem.*` | ensure → bindConfig → restore → `agent->update` |
| `../bt/conditions/CondAI.*` | Patrol / Chase / Alert / Jostled / PathFinding 条件 |
| `../bt/actions/AIActions.*` | 对应叶子；读写 `AiAgent::of(entity)` |
| `../debug/WorldHash.*` | 混入 agent 标量 + 各 SkillAi 字段 |

## 对象图

- `SkillAi`：`ensure` / `tick` / `check`。check 顺序：loadCd → useCount → checkCd → composition → 置 checkCd → 概率 → 扣次数。
- `AiAgent`：按 `roleConfig->aiIds.front()` 重建 `SkillAi` 列表（拓扑以表为准）。`update` 在 chase 范围内收集间隔已好的槽，按 `skillPriorityLevel` 从高到低选，同权按表序；`SkillAi::check` 只在尝试该槽时调用。施放成功后重置同权槽的 `skillPriorityLevelCd`。
- 不设 `AiFactory`（类型只有两种）。

## 快照

1. `bindConfig` 按表重建 `SkillAi` 列表。
2. `restoreRuntimeData()` 按 `typeName` + 标量填回已 bind 的列表（个数或 id 对不上则断言）。
3. `checkCdRemainMs` / `skillSlotIntervalMs` / 巡逻目标活在对象上。

## 与行为树

角色树 AI 枝顺序不变：Jostled → Patrol → Chase → Alert → PathFinding。Attack 条件通过时 AI 移动条件失败（施法中不巡逻）。系统顺序：`AISystem` 在 `BehaviorTreeSystem` 之前。
