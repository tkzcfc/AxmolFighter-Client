#pragma once
#include "EntityGroup.h"
#include "input/ControlEventQueue.h"
#include "role/RoleComponents.h"
#include "role/brain/RoleBrain.h"
#include "skill/SkillComponents.h"
#include "effect/Effect.h"
#include "buff/BuffComponents.h"
#include "ability/AbilityComponents.h"
#include "ai/AiAgent.h"
#include "room/RoomEntities.h"
#include <tuple>

// 生产世界允许挂载的组件注册表，顺序固定类型标识。
#define MG_COMPONENT_LIST(X)                                                                              \
    X(EntityGroup)                                                                                        \
    X(ControlEventQueue) X(RoleIdentity) X(RoleStatus) X(ExtraState) X(Attributes) X(Vitals) X(Transform) \
        X(MotionState) X(Puppet) X(ColliderState) X(CollisionState) X(RoleTimers) X(RoleHitState) X(Displacement) X(RoleBrain) X(RoleControl) X(SkillPool) X(EffectState) X(EffectTree) X(BuffPool) X(BuffExternalConditions) X(AbilityPool) X(AiAgent) X(PortalState) X(ObstacleState) X(GoodsState) X(DropGoods)
namespace mugen::sim
{
// 角色脑与控制组件位于 role::brain，此处引入便于组件注册表展开。
using role::brain::RoleBrain;
using role::brain::RoleControl;
#define MG_SIM_COMPONENT_TUPLE(T) \
    , std::tuple<T> {}
// 由注册表展开的 ECS 组件元组。
using Components = decltype(std::tuple_cat(std::tuple<> {} MG_COMPONENT_LIST(MG_SIM_COMPONENT_TUPLE)));
#undef MG_SIM_COMPONENT_TUPLE
}  // namespace mugen::sim
