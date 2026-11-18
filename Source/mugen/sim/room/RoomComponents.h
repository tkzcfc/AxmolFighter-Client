#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/core/reflect/Reflect.h"
#include <vector>

namespace mugen::sim
{
// 传送门三阶段：关闭、开放（可进入）、传送中。
enum class PortalPhase : uint8_t
{
    // 未开放，碰撞仅推开角色。
    Close = 0,
    // 已开放，胜利后碰撞触发传送。
    Active = 1,
    // 已进入传送，等待房间切换。
    Transfer = 2
};
// 掉落物阶段：抛射、待机、收集。
enum class GoodsPhase : uint8_t
{
    // 抛出后飞行中。
    Spurt = 0,
    // 落地静止等待拾取。
    Idle = 1,
    // 被拾取，播放收集动作后销毁。
    Collect = 2
};

// 障碍物状态：按剩余受击次数销毁，不按生命值。
struct ObstacleStateComponent
{
    // 障碍配置标识。
    int32_t configId = 0;
    // 剩余受击次数；负一表示不可破坏。
    int32_t hits = -1;
    // 状态动作已运行毫秒。
    Fixed timeMs;
    // 死亡位移只初始化一次，等待制动或落地。
    bool deathStarted = false;
    bool displacementEnd = false;
    // 障碍等级（计算属性用）。
    int32_t level = 1;
    MG_REFLECT(ObstacleStateComponent, configId, hits, timeMs, deathStarted, displacementEnd, level)
};

// 掉落物状态：携带抛射速度与拾取者。
struct GoodsStateComponent
{
    // 物品配置标识。
    int32_t configId = 0;
    // 当前阶段。
    GoodsPhase phase = GoodsPhase::Idle;
    // 抛射速度（仅 Spurt 阶段）。
    FixedVec3 velocity;
    // 拾取者句柄。
    Entity collector;
    MG_REFLECT(GoodsStateComponent, configId, phase, velocity, collector)
};

// 传送门状态：目标房间与三阶段。
struct PortalStateComponent
{
    // 门配置标识。
    int32_t configId = 0;
    // 目标房间、目标朝向与限制类型。
    int32_t roomId = 0;
    int32_t toward = 1;
    int32_t limit = 0;
    // 到达位置与当前阶段。
    FixedVec3 destination;
    PortalPhase phase = PortalPhase::Close;
    MG_REFLECT(PortalStateComponent, configId, roomId, toward, limit, destination, phase)
};

// 死亡掉落配置：怪物与障碍死亡时结算。
struct DropTableComponent
{
    // 已解析的掉落实体配置，允许相同配置重复出现。
    std::vector<int32_t> goods;
    // 生命与能量药品掉落百分比（千分比语义沿用旧实现：与 0–100 随机数比较）。
    Fixed hpRate;
    Fixed epRate;
    MG_REFLECT(DropTableComponent, goods, hpRate, epRate)
};
}  // namespace mugen::sim
