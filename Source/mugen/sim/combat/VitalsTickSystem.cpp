// [2.3] VitalsTickSystem
// 读：`Health`、`Stats`、`RoleState`、`SkillCast`、`Identity`
// 写：MP 回复、暴气 EP、`MoveClock`、生命/能量采样、`SkillCast.m_crazy`
// 消费：—
// 产生：同步触发 AfterCrazy（暴气能量耗尽）
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>

namespace mugen::sim
{
// 系统 VitalsTickSystem：2.3 行。MP 回复、暴气消耗、走跑/站立计时与资源采样。
void runVitalsTick(World& world)
{
    const Fixed dtMs = world.ecs().singleton<WorldClock>().m_dtMs;
    const Fixed dtSec = world.ecs().singleton<WorldClock>().m_dtSec;
    world.ecs().each<Health, RoleState>([&](Entity entity, Health& health, RoleState& state) {
        if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
            return;
        const Fixed scale = world.ecs().has<Stats>(entity) ? world.ecs().get<Stats>(entity).finalStat(203)
                                                           : Fixed::fromInt(1);
        health.m_mp = std::min(health.m_maxMp, health.m_mp + dtMs * conf::kMpIncreaseBasicValue * scale);
        // 跑动同时带站立位，状态位不会恰好等于 Run，连续移动秒数保持为 0。
        state.m_movedSec =
            role::statusBits(state.m_state) == EntityRoleStatus::Run ? state.m_movedSec + dtSec : Fixed{};
        state.m_standingSec = state.m_state == RoleStateId::Idle ? state.m_standingSec + dtSec : Fixed{};
        health.m_prevHp = health.m_hp;
        health.m_prevEp = health.m_ep;
        if (!world.ecs().has<SkillCast>(entity) || !world.ecs().get<SkillCast>(entity).m_crazy)
            return;
        Fixed extra{};
        if (const auto* identity = world.ecs().tryGet<Identity>(entity))
        {
            const int32_t id = identity->m_configId;
            if (id >= 1 && id <= 4)
            {
                const Fixed additions[] = {conf::kCrazyDecayValueRole1, conf::kCrazyDecayValueRole2,
                                           conf::kCrazyDecayValueRole3, conf::kCrazyDecayValueRole4};
                extra                    = additions[id - 1];
            }
        }
        health.m_ep = std::max(Fixed{}, health.m_ep - dtMs * (conf::kCrazyDecayBasicValue + extra));
        if (health.m_ep > Fixed{})
            return;
        // 冻结实现里槽位变化默认已置位，能量归零即结束暴气。
        world.ecs().get<SkillCast>(entity).m_crazy = false;
        BuffContext ctx;
        ctx.m_target = entity;
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::AfterCrazy, ctx);
    });
}
}  // namespace mugen::sim
