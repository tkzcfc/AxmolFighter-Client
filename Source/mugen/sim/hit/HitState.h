#pragma once
#include "mugen/sim/role/RoleComponents.h"
#include <algorithm>

namespace mugen::sim { class World; }
namespace mugen::sim::hit
{
// 查询实体当前是否触发浮空保护。
bool isRigidity(World&, Entity);
// 读取并扣减实体硬直。
Fixed getStiffTime(World&, Entity);
// 扣除浮空值并按连击重量加速下落。
bool dealWithWeight(World&, Entity, Fixed, bool hitProtect = false);
// 初始化仅在竞技模式生效的生命保护。
void hitProtectInit(World&, Entity);
// 追击时刷新保护清理倒计时。
void updateHitProtectTime(World&, Entity);
// 按逻辑毫秒推进保护并检测生命阈值。
void dealHitProtectUpdate(World&, Entity);
// 醒来或超时后清除保护记录。
void resetHitProtect(World&, Entity);
// 只有配置了正浮空总值的角色才会触发浮空保护。
inline bool isRigidity(const RoleHitState& state, Fixed configuredRigidity)
{
    return state.rigidity <= Fixed{} && configuredRigidity > Fixed{};
}

// 读取硬直时逐次扣除疲劳，死亡或未触发保护时保持原值。
inline Fixed getStiffTime(RoleHitState& state, Fixed configuredRigidity, bool dead)
{
    if (!dead && isRigidity(state, configuredRigidity))
        state.stiffTime = std::max(Fixed{}, state.stiffTime - state.fatigue * Fixed::fromInt(state.hitCounts));
    return state.stiffTime;
}
}
