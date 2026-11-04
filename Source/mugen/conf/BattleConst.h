#pragma once

#include "mugen/core/math/Fixed.h"

#include <array>

namespace mugen::conf
{
// 周期伤害默认等级加成，Const.lua:616，值为 0。
inline constexpr Fixed kBuffHurtDefaultAddition = "0"_fx;
// 每次更新施加的重力加速度。
inline constexpr Fixed kGravity = "-0.0022"_fx;
// 深度方向速度缩放。
inline constexpr Fixed kZRate = "0.55"_fx;
// 落地弹跳衰减系数。
inline constexpr Fixed kBounces = "0.2"_fx;
// 步行速度倍率。
inline constexpr Fixed kWalkRate = "1"_fx;
// 跑步速度倍率。
inline constexpr Fixed kRunRate = "2.2"_fx;
// 步行动画播放倍率。
inline constexpr Fixed kWalkScaleTime = "1.2"_fx;
// 跑步动画播放倍率。
inline constexpr Fixed kRunScaleTime = "1"_fx;
// 待机动画播放倍率。
inline constexpr Fixed kIdleScaleTime = "1"_fx;
// 受击动画倍率，Const.lua:234，值为 1。
inline constexpr Fixed kHitScaleTime = "1"_fx;
// 浮空动画倍率，Const.lua:236，值为 1。
inline constexpr Fixed kHitUpScaleTime = "1"_fx;
// 下落动画倍率，Const.lua:238，值为 1。
inline constexpr Fixed kHitDownScaleTime = "1"_fx;
// 倒地动画倍率，Const.lua:240，值为 1。
inline constexpr Fixed kHitFloorScaleTime = "1"_fx;
// 起身动画倍率，Const.lua:242，值为 1。
inline constexpr Fixed kGetUpScaleTime = "1"_fx;
// 每毫秒基础被动魔力回复。
inline constexpr Fixed kMpIncreaseBasicValue = "0.005"_fx;
// 各职业额外被动魔力回复。
inline constexpr Fixed kMpIncreaseValueRole1 = "0"_fx;  // 职业一额外回复。
inline constexpr Fixed kMpIncreaseValueRole2 = "0"_fx;  // 职业二额外回复。
inline constexpr Fixed kMpIncreaseValueRole3 = "0"_fx;  // 职业三额外回复。
inline constexpr Fixed kMpIncreaseValueRole4 = "0"_fx;  // 职业四额外回复。
// 按受伤百分比获得的怒气值。
inline constexpr Fixed kCrazyFromHurtValue = "15"_fx;
// 每毫秒基础怒气衰减。
inline constexpr Fixed kCrazyDecayBasicValue = "0.0666"_fx;
// 各职业额外怒气衰减。
inline constexpr Fixed kCrazyDecayValueRole1 = "0"_fx;  // 职业一额外衰减。
inline constexpr Fixed kCrazyDecayValueRole2 = "0"_fx;  // 职业二额外衰减。
inline constexpr Fixed kCrazyDecayValueRole3 = "0"_fx;  // 职业三额外衰减。
inline constexpr Fixed kCrazyDecayValueRole4 = "0"_fx;  // 职业四额外衰减。
// 连携技能的魔力消耗倍率。
inline constexpr Fixed kLinkSkillMpRate = "0.8"_fx;
// 附加冷却时间倍率。
inline constexpr Fixed kColdTimeSale = "1"_fx;
// 跑步行为的打断优先级。
inline constexpr int32_t kRunSorder = 30;
// 技能等级伤害的默认附加值。
inline constexpr Fixed kSkillHurtDefaultAddition = "0"_fx;
// 敌方起身无敌效果 id。
inline constexpr int32_t kRoleEnemyWakeBuffId = -1;
// 角色起身无敌效果 id。
inline constexpr int32_t kRoleHeroWakeBuffId = 201;
// 角色死亡延迟毫秒数。
inline constexpr int32_t kRoleDeathTime = 0;
// 重新选择目标的间隔秒数。
inline constexpr int32_t kChangeTargetTime = 10;
// 摇杆指令等待间隔秒数。
inline constexpr Fixed kJoystickWaitTime = "0.35"_fx;
// 伙伴怒气变身怒气上限。
inline constexpr Fixed kPartnerRageMaxAnger = "100"_fx;
// 房间状态内部切换持续帧数。
inline constexpr int32_t kRoomStateInternal = 30;
// 受击保护的生命阈值比例。
inline constexpr Fixed kHitProtectHpScale = "0.45"_fx;
// 受击保护清理间隔毫秒数。
inline constexpr int32_t kHitProtectCleanTime = 5000;
// 连击未命中后重置连击数的超时毫秒数。
inline constexpr Fixed kComboTimeMs = "2000"_fx;
// 连击评价等级的经验阈值，按评价等级升序。
inline constexpr std::array<Fixed, 5> kComboStageExp = {"200"_fx, "500"_fx, "1000"_fx, "1700"_fx, "2500"_fx};
// 连击评价等级每帧的经验衰减速度，与经验阈值一一对应。
inline constexpr std::array<Fixed, 5> kComboStageSpeed = {"0.5"_fx, "0.7"_fx, "1.8"_fx, "3.4"_fx, "5"_fx};
// 击杀目标额外增加的经验值。
inline constexpr Fixed kComboKillExp = "50"_fx;
// 命中眩晕目标额外增加的经验值。
inline constexpr Fixed kComboStunExp = "50"_fx;
// 首领命中的经验倍率。
inline constexpr Fixed kComboBossExpRate = "3"_fx;
// 连击公式固定参数：连击数阈值。
inline constexpr std::array<int32_t, 4> kComboFormulaCombo = {50, 100, 175, 250};
// 连击公式固定参数：与阈值对应的加数 M。
inline constexpr std::array<Fixed, 4> kComboFormulaM = {"0"_fx, "1"_fx, "2"_fx, "3"_fx};
// 连击公式固定参数：与阈值对应的倍率 N。
inline constexpr std::array<Fixed, 4> kComboFormulaN = {"1.2"_fx, "1.3"_fx, "1.4"_fx, "1.5"_fx};
}  // namespace mugen::conf
