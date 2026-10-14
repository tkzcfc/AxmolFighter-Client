#include "Status.h"
#include "mugen/sim/World.h"
#include "mugen/sim/hit/HitState.h"
#include "mugen/sim/trace/TraceSink.h"

namespace mugen::sim::status
{
// 按状态机规则应用转换，死亡角色拒绝后续转换。
bool dealWithStatus(World& world, Entity entity, EntityRoleStatus bits, int32_t id, FixedVec3 movePos)
{
    auto& role = world.ecs().get<RoleStatus>(entity);
    if (role.isDeath())
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
        trace::noteDeath(world, entity);
        break;
    case S::Jostled:
        role.entityParams.displacement_id = id;
        role.mix(bits);
        break;
    case S::Patrol:
    case S::Finding:
    case S::Chase:
    case S::Alert:
        role.entityParams.move_pos = movePos;
        if (role.eEntityStatus != S::Idle)
            return false;
        role.mix(bits);
        break;
    case S::Walk:
    case S::Run:
        role.abort(S::Finding);
        if (role.eEntityStatus != S::Idle)
            return false;
        role.mix(bits);
        break;
    case S::Idle:
        if (!role.isMove() || role.isAttack())
            return false;
        role.eEntityStatus = S::Idle;
        break;
    default:
        role.mix(bits);
        break;
    }
    return true;
}
}  // namespace mugen::sim::status
