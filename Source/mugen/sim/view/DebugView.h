#pragma once

#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/math/FixedVec3.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace mugen::sim
{
class World;

namespace view
{
// 调试碰撞框，字段与世界轴对齐盒一致。
struct DebugBox
{
    // 三轴最小角。
    FixedVec3 pos;
    // 三轴尺寸。
    FixedVec3 size;
};

// 单个实体的碰撞调试快照。
struct ColliderDebugView
{
    // 实体句柄。
    Entity entity;
    // 世界坐标。
    FixedVec3 position;
    // 行走与贴墙半径。
    Fixed radius;
    // 攻击框。
    std::vector<DebugBox> attack;
    // 受击框。
    std::vector<DebugBox> damage;
};

// 角色调试快照。
struct RoleDebugView
{
    // 实体句柄。
    Entity entity;
    // 角色配置标识。
    int32_t roleId = 0;
    // 角色等级。
    int32_t level = 1;
    // 当前状态编号。
    int32_t state = 0;
    // 世界坐标。
    FixedVec3 position;
};

// 地图可移动区域（字段与 MapRegion 一致，避免头文件依赖 sim/motion）。
struct MapMoveRangeView
{
    // 矩形左下角 X。
    Fixed x;
    // 矩形左下角 Y。
    Fixed y;
    // 矩形宽度。
    Fixed width;
    // 矩形高度。
    Fixed height;
};

// 当前逻辑帧号。
uint64_t debugFrame(const World& world);
// 当前世界哈希。
uint64_t debugHash(const World& world);
// 按玩家槽位取控制实体。
Entity playerEntity(const World& world, uint32_t slot);
// 收集当前碰撞框，跳过待回收与休眠实体。
void collectColliders(const World& world, std::vector<ColliderDebugView>& out);
// 收集地图可移动区域。
void moveRanges(const World& world, std::vector<MapMoveRangeView>& out);
// 收集角色调试快照，跳过待回收与休眠实体。
void collectRoleDebug(const World& world, std::vector<RoleDebugView>& out);
// 调试面板 Cast：对空闲或可移动角色按槽位开始施法。
bool debugCastSlot(World& world, Entity player, std::string_view poolSlot);
}  // namespace view
}  // namespace mugen::sim
