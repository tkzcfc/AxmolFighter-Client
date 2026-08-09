# Behavior Tree（技能 / 角色）

对齐黑月行为树驱动的技能系统。树结构不序列化；快照恢复后由 `RoleTreeBuilder::attachToEntity` 重建，路径由组件上下文重选。

## 目录

| 路径 | 作用 |
|------|------|
| `RoleTreeBuilder.*` | 角色根树（Death→…→Attack→移动） |
| `SkillTreeBuilder.*` | Attack 下 Slot/Step/Pipe/Toward 子树 |
| `SkillCastRules.*` | castBegan/预输入/取消窗/朝向（黑月 SkillPool） |
| `actions/` | Locomo / HitReact / AttackAction |
| `conditions/` | CondStatus / CondAttack* |
| `../core/bt/` | Selector/Sequence/Action 引擎 |

## 组件职责

- `BehaviorTreeComponent`：树指针 + sticky memory + 动作计时 / effectSpawnMask
- `SkillCastComponent`：当前/预输入技能、管道三态、costPaid
- `HitReactComponent`：受击队列
- `BehaviorComponent`：状态位 / 移动与硬直（施法字段已迁出）

## 系统顺序

`CombatSystem` → `BehaviorTreeSystem` → `DisplacementSystem`
