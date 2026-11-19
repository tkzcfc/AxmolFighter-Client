#pragma once

#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/sim/WorldSetup.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mugen::sim
{
class World;
struct PresentationEvent;

// 表现层可见实体的种类。召唤物与角色共用 Role。
enum class VisualEntityKind : uint8_t
{
    // 角色或召唤物。
    Role = 0,
    // 特效。
    Effect = 1,
    // 障碍。
    Obstacle = 2,
    // 物品。
    Goods = 3,
    // 传送门。
    Portal = 4,
};

// 表现层遍历用的可视实体快照。
struct VisualEntityView
{
    // 实体句柄。
    Entity entity;
    // 可视种类。
    VisualEntityKind kind = VisualEntityKind::Role;
    // 配置标识。
    int32_t configId = 0;
    // 世界坐标。
    FixedVec3 position;
    // 水平朝向，仅取 -1 或 1。
    int32_t towardX = 1;
    // 纵深朝向向量。
    Fixed vectorZ = Fixed::fromInt(1);
    // 动作文件。
    std::string motionFile;
    // 当前动作名称。
    std::string motionName;
    // 当前动作入口标识。
    std::string entryId;
    // 是否循环播放。
    bool loop = true;
    // 当前动作播放时间。
    Fixed timeMs;
    // 动作播放倍率；顿帧或定身时为 0。
    Fixed actionScale = Fixed::fromInt(1);
    // 伙伴怒气是否处于变身。
    bool partnerRageTransformed = false;
};

// 实体角色身份只读摘要。
struct EntityRoleInfoView
{
    // 是否读到有效角色身份。
    bool valid = false;
    // 角色配置标识。
    int32_t roleId = 0;
    // 角色类型。
    int32_t roleType = 0;
    // 阵营。
    int32_t camp = 0;
    // 伙伴怒气是否处于变身。
    bool partnerRageTransformed = false;
};

// 城镇角色移动只读摘要。
struct TownMovementView
{
    // 是否读到有效移动状态。
    bool valid = false;
    // 世界坐标。
    FixedVec3 position;
    // 当前速度。
    FixedVec3 velocity;
    // 水平朝向，仅取 -1 或 1。
    int32_t towardX = 1;
    // 当前动作名称。
    std::string motionName;
    // 是否处于走或跑。
    bool moving = false;
};

namespace view
{
// 当前已推进的逻辑帧号。
uint64_t worldFrame(const World& world);
// 当前逻辑帧的毫秒时长。
Fixed worldDtMs(const World& world);
// 固定逻辑帧的毫秒数。
Fixed logicDtMs();
// 上一帧产生的表现事件。
const std::vector<PresentationEvent>& worldEvents(const World& world);
// 收集当前可视实体，跳过待回收与休眠实体。
void collectVisualEntities(const World& world, std::vector<VisualEntityView>& out);
// 返回槽位对应的玩家实体；越界或不存在时返回空实体。
Entity playerEntity(const World& world, uint32_t slot);
// 收集当前可见角色与召唤物的世界坐标，供回滚纠偏采样。
void collectRolePositions(const World& world, std::vector<std::pair<Entity, FixedVec3>>& out);
// 读取实体世界坐标；不可见或缺失变换时返回假。
bool readEntityPosition(const World& world, Entity entity, FixedVec3& outPos);
// 读取实体角色身份。
EntityRoleInfoView readEntityRoleInfo(const World& world, Entity entity);
// 立即生成城镇可控角色并返回实体。
Entity spawnTownRole(World& world, int32_t roleId, int32_t level, const FixedVec3& position, int32_t towardX,
                     std::optional<uint32_t> playerSlot);
// 立即生成城镇远端木偶并返回实体。
Entity spawnTownPuppet(World& world, int64_t playerId, int32_t roleId, const FixedVec3& position, int32_t towardX);
// 立即生成城镇常开传送门并返回实体（仅展示与位置，触发逻辑由城镇界面处理）。
Entity spawnTownPortal(World& world, int32_t portalId, const FixedVec3& position);
// 从玩家槽位移除并销毁城镇实体。
void destroyTownEntity(World& world, Entity entity);
// 读取城镇实体移动状态。
TownMovementView readTownMovement(const World& world, Entity entity);
// 写入城镇实体世界坐标。
void setTownEntityPosition(World& world, Entity entity, const FixedVec3& position);
// 收集角色战斗中会引用的特效 Spine 资源 id，升序去重。
std::vector<int32_t> collectRoleEffectSpineIds(int32_t roleId, int32_t aiDifficulty = 0);
}  // namespace view
}  // namespace mugen::sim
