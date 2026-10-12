#include "Status.h"
#include "mugen/sim/World.h"
#include "mugen/sim/hit/HitState.h"

namespace mugen::sim::status
{
// 检查请求的全部状态位是否存在。
bool isStatus(const RoleStatus& role, EntityRoleStatus bits)
{
    return (static_cast<uint32_t>(role.eEntityStatus) & static_cast<uint32_t>(bits)) == static_cast<uint32_t>(bits);
}
// 将状态位并入角色状态。
void mixStatus(RoleStatus& role, EntityRoleStatus bits)
{
    role.eEntityStatus = role.eEntityStatus | bits;
}
// 仅在已存在时清除状态位。
void abortStatus(RoleStatus& role, EntityRoleStatus bits)
{
    if (isStatus(role, bits))
        role.eEntityStatus =
            static_cast<EntityRoleStatus>(static_cast<uint32_t>(role.eEntityStatus) ^ static_cast<uint32_t>(bits));
}
// 聚合所有移动类状态判断。
bool isMoveStatus(const RoleStatus& r)
{
    return isPatrolStatus(r) || isChaseStatus(r) || isAlertStatus(r) || isRunStatus(r) || isWalkStatus(r) ||
           isFindingStatus(r);
}
// 聚合所有可被打断的受击状态判断。
bool isCanBreakStatus(const RoleStatus& r)
{
    return isHitFloorStatus(r) || isHitDownStatus(r) || isHitUpStatus(r) || isHitSwitchStatus(r) || isHitStatus(r);
}
// 判断起身或可打断状态是否允许受击。
bool isTobeHitStatus(const RoleStatus& r)
{
    return isGetUpStatus(r) || isCanBreakStatus(r);
}
// 按状态机规则应用转换，死亡角色拒绝后续转换。
bool dealWithStatus(World& world, Entity entity, EntityRoleStatus bits, int32_t id, FixedVec3 movePos)
{
    auto& role = world.ecs().get<RoleStatus>(entity);
    if (isDeathStatus(role))
        return false;
    using S = EntityRoleStatus;
    switch (bits)
    {
    case S::Hit:
        role.eEntityStatus                                  = S::Idle | S::Hit;
        role.entityParams.hit_id                            = id;
        role.entityParams.hit_counts                        = 1;
        world.ecs().get<RoleHitState>(entity).hitCounts = 1;
        world.ecs().get<RoleHitState>(entity).forceVelocity = {};
        hit::hitProtectInit(world, entity);
        break;
    case S::Death:
        role.eEntityStatus                      = S::Death;
        role.entityParams.death_displacement_id = id;
        break;
    case S::Jostled:
        role.entityParams.displacement_id = id;
        mixStatus(role, bits);
        break;
    case S::Patrol:
    case S::Finding:
    case S::Chase:
    case S::Alert:
        role.entityParams.move_pos = movePos;
        if (role.eEntityStatus != S::Idle)
            return false;
        mixStatus(role, bits);
        break;
    case S::Walk:
    case S::Run:
        abortStatus(role, S::Finding);
        if (role.eEntityStatus != S::Idle)
            return false;
        mixStatus(role, bits);
        break;
    case S::Idle:
        if (!isMoveStatus(role) || isAttackStatus(role))
            return false;
        role.eEntityStatus = S::Idle;
        break;
    default:
        mixStatus(role, bits);
        break;
    }
    return true;
}
}  // namespace mugen::sim::status
