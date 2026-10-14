#pragma once
#include "mugen/sim/role/RoleComponents.h"
#include "mugen/core/ecs/Entity.h"

namespace mugen::sim
{
// 状态系统所更新的世界实例。
class World;
namespace status
{
// 按状态转换规则处理请求，并在成功时写入附带参数。
bool dealWithStatus(World& world, Entity entity, EntityRoleStatus bits, int32_t id = 0, FixedVec3 movePos = {});
}  // namespace status
}  // namespace mugen::sim
