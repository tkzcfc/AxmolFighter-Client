// [3.1] AiSystem
// 读：`AiBrain`、`TransformComponent`、`HealthComponent`、`RoleStateComponent`、`SkillBookComponent`、`TauntTableComponent`、`HitStateComponent`(受击记录)
// 写：`IntentComponent`、`AiBrain`；RNG
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/ai/AiRules.h"

namespace mugen::sim
{
// 系统 AiSystem：3.1 行，见 DESIGN.md §3.2。AI 关闭时完全跳过。
// 怪物 AI 只写 IntentComponent，与玩家走同一施法/移动路径，不直接改 FSM。
void runAi(World& world)
{
    if (!world.setup().m_aiEnabled)
        return;
    world.ecs().each<IdentityComponent, TransformComponent>(
        [&](Entity entity, const IdentityComponent& identity, const TransformComponent&) {
            if (identity.m_kind != SpawnKind::Role)
                return;
            // 玩家槽位角色（含离线玩家）由输入驱动，不参与怪物 AI；与旧实现 isAi=false 语义一致。
            if (identity.m_playerSlot.has_value())
                return;
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            // 难度已随 SpawnRequest 落盘到 IdentityComponent（出生时写），这里按实体的难度解析 AiDef。
            ai::rules::ensureBrain(world, entity, identity.m_aiDifficulty);
            ai::rules::updateAgent(world, entity);
        });
}
}  // namespace mugen::sim
