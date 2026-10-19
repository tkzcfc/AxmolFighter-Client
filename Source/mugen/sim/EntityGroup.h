#pragma once
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim
{
// 实体组类型，决定世界中的系统更新顺序。
enum class GroupKind : uint8_t
{
    // 角色实体组。
    Role,
    // 特效实体组。
    Effect,
    // 障碍物实体组。
    Obstacle,
    // 掉落物实体组。
    Goods,
    // 传送门实体组。
    Portal,
    // 组类型计数哨兵。
    Count
};
// 世界每帧处理实体组的固定顺序。
inline constexpr std::array kGroupOrder{GroupKind::Role, GroupKind::Effect, GroupKind::Obstacle, GroupKind::Goods,
                                        GroupKind::Portal};
// 标记实体所属组及其生命周期控制状态。
struct EntityGroup
{
    // 实体参与的更新组。
    GroupKind kind = GroupKind::Role;
    // 暂停组内常规更新。
    bool suspended = false;
    // 终止组内行为但保留实体。
    bool stopped = false;
    // 已请求在帧末销毁实体。
    bool destroyRequested = false;
    // 参与组件快照的组状态字段。
    MG_REFLECT(EntityGroup, kind, suspended, stopped, destroyRequested)
};
}  // namespace mugen::sim
