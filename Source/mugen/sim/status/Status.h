#pragma once
#include "mugen/sim/role/RoleComponents.h"
#include "mugen/core/ecs/Entity.h"

namespace mugen::sim
{
// 状态系统所更新的世界实例。
class World;
namespace status
{
// 判断角色是否同时拥有指定状态位。
bool isStatus(const RoleStatus& role, EntityRoleStatus bits);
// 合并指定状态位。
void mixStatus(RoleStatus& role, EntityRoleStatus bits);
// 移除已设置的指定状态位。
void abortStatus(RoleStatus& role, EntityRoleStatus bits);
// 按状态转换规则处理请求，并在成功时写入附带参数。
bool dealWithStatus(World& world, Entity entity, EntityRoleStatus bits, int32_t id = 0, FixedVec3 movePos = {});
// 为每个状态位生成查询函数。
#define MG_ROLE_STATUS_QUERY(Name)                       \
    inline bool is##Name##Status(const RoleStatus& role) \
    {                                                    \
        return isStatus(role, EntityRoleStatus::Name);   \
    }
MG_ROLE_STATUS_QUERY(None)
MG_ROLE_STATUS_QUERY(Birth)
MG_ROLE_STATUS_QUERY(Death)
MG_ROLE_STATUS_QUERY(Wake)
MG_ROLE_STATUS_QUERY(GetUp)
MG_ROLE_STATUS_QUERY(HitFloor)
MG_ROLE_STATUS_QUERY(HitDown)
MG_ROLE_STATUS_QUERY(HitUp)
MG_ROLE_STATUS_QUERY(HitSwitch)
MG_ROLE_STATUS_QUERY(Hit)
MG_ROLE_STATUS_QUERY(Attack)
MG_ROLE_STATUS_QUERY(Fall)
MG_ROLE_STATUS_QUERY(Patrol)
MG_ROLE_STATUS_QUERY(Chase)
MG_ROLE_STATUS_QUERY(Alert)
MG_ROLE_STATUS_QUERY(Run)
MG_ROLE_STATUS_QUERY(Walk)
MG_ROLE_STATUS_QUERY(Idle)
MG_ROLE_STATUS_QUERY(Finding)
MG_ROLE_STATUS_QUERY(Jostled)
MG_ROLE_STATUS_QUERY(Revive)
MG_ROLE_STATUS_QUERY(Controlled)
#undef MG_ROLE_STATUS_QUERY
// 判断角色是否处于移动类状态。
bool isMoveStatus(const RoleStatus& role);
// 判断角色是否处于可受击状态。
bool isTobeHitStatus(const RoleStatus& role);
// 判断角色是否处于可被打断状态。
bool isCanBreakStatus(const RoleStatus& role);
}  // namespace status
}  // namespace mugen::sim
