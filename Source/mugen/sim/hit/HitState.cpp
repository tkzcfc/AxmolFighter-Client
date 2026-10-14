#include "HitState.h"
#include "Hit.h"
#include "HitSeams.h"
#include "mugen/sim/World.h"
#include "mugen/sim/status/Status.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::hit
{
// 配置总值与运行剩余值分别参与保护判断。
bool isRigidity(World& world, Entity entity)
{
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(world.ecs().get<RoleIdentity>(entity).roleId);
    return row && world.ecs().get<RoleHitState>(entity).isRigidity(Fixed::fromInt(row->rigidity));
}
// 保留读取本身扣减硬直的语义。
Fixed getStiffTime(World& world, Entity entity)
{
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(world.ecs().get<RoleIdentity>(entity).roleId);
    return world.ecs().get<RoleHitState>(entity).getStiffTime(
        row ? Fixed::fromInt(row->rigidity) : Fixed{}, isDead(world, entity));
}
// 耗尽后每次追击都加重下落，保护提示只向 Buff 接缝发送请求。
bool dealWithWeight(World& world, Entity entity, Fixed amount, bool hitProtect)
{
    auto& state = world.ecs().get<RoleHitState>(entity);
    state.rigidity = std::max(Fixed{}, state.rigidity - amount);
    if (isDead(world, entity) || !isRigidity(world, entity)) return false;
    state.forceVelocity.y -= state.weight * Fixed::fromInt(state.hitCounts);
    if (world.ecs().get<RoleIdentity>(entity).roleType == 1)
    {
        const auto query = world.hitHooks().m_hasBuff;
        if (!query || (!query(world, entity, 155) && !query(world, entity, 173)))
        {
            buff::addEntityBuff(world, entity, hitProtect ? 173 : 155);
            buff::addEntityBuff(world, entity, 154);
        }
    }
    return true;
}
// 首次命中记录当前生命减去最大生命比例后的阈值。
void hitProtectInit(World& world, Entity entity)
{
    auto& state = world.ecs().get<RoleHitState>(entity);
    const auto& vitals = world.ecs().get<Vitals>(entity);
    state.initHitProtect(world.setup().m_pvp, vitals.hp, vitals.maxHp, conf::kHitProtectHpScale,
                         conf::kHitProtectCleanTime);
}
// 已执行的保护不再因追击延长。
void updateHitProtectTime(World& world, Entity entity)
{
    world.ecs().get<RoleHitState>(entity).updateHitProtect(conf::kHitProtectCleanTime);
}
// 清除本轮保护状态，下一次命中可重新建立阈值。
void resetHitProtect(World& world, Entity entity)
{
    world.ecs().get<RoleHitState>(entity).resetHitProtect();
}
// 先处理超时，再检测血量；仅受击中的角色强制耗尽浮空值。
void dealHitProtectUpdate(World& world, Entity entity)
{
    auto& state = world.ecs().get<RoleHitState>(entity);
    if (!state.m_hitProtectHp || state.m_hitProtectExecuted) return;
    state.m_hitProtectTime -= world.ecs().singleton<WorldClock>().dtMs;
    if (state.m_hitProtectTime <= Fixed{}) { state.resetHitProtect(); return; }
    if (world.ecs().get<Vitals>(entity).hp <= *state.m_hitProtectHp)
    {
        if (world.ecs().get<RoleStatus>(entity).isTobeHit())
            dealWithWeight(world, entity, state.rigidity, true);
        state.m_hitProtectExecuted = true;
    }
}
}
