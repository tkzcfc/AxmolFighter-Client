# Buff（Buff / BuffManager）

Buff 是 `Object` 图：`BuffManager` 持有 `Buff` 列表和无敌/霸体/眩晕引用计数。`BuffComponent` 只持有 manager。规则仍是无状态 flyweight（`BuffRuleBase` + `BuffRuleFactory`），不把每条规则收成 Buff 子类。

当前副本已注册施加器 / 按状态加 Buff / 改 MP；未挂的 className 不预先实现。`BuffSneer` 仍为空桩。SpecialAbility 后置。

## 目录

| 路径 | 作用 |
|------|------|
| `Buff.*` | 单条 buff：剩余时间、叠层、周期、已写入量 |
| `BuffManager.*` | add / remove / trigger / 到期卸 |
| `BuffRuleBase.*` / `BuffRules.*` / `BuffRuleFactory.*` | className 共享规则 |
| `BuffRuleUtil.*` | 触发门控、扩展属性、Spine |
| `BFEvent.h` | began / ended 数值 |
| `../component/BuffComponent.*` | 快照容器：manager + pending blob |
| `../system/BuffSystem.*` | ensure → restore → `manager->update` |
| `../debug/WorldHash.*` | 混入 refs + 各 Buff 的 id / remainingMs / repeatCount |

## 对象图

- `Buff`：`tick` 推进 innerCd、规则 `onTick`、默认周期伤、剩余时间；到期置 `destroyed`。
- `BuffManager`：叠层 / `removeRepeatAll` / began-ended 分发语义与原先静态 API 相同。无 `bindConfig`（列表是运行时拓扑）。
- 不设 `BuffFactory`（实例类型只有一种）。

## 快照

1. 序列化写出 refs + `buffCount` + 每个 `Buff` 的 `typeName` + 标量。
2. `restoreRuntimeData()` **重建** `unique_ptr<Buff>` 列表（`typeName` 对不上则断言）。
3. `remainingMs` / `repeatCount` / `applied` / 三套 ref 进出快照后续得上且不重套属性。

## 与系统

系统/叶子只调 `BuffManager::of(entity)`，不读组件 buff 裸字段。
