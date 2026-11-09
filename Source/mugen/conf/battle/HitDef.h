#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf::battle
{
// 命中反应配置，由 skill_hit.lua 降级，字段原样、单位统一（见 DESIGN.md §6.6）。
struct HitDef
{
    // 配置行 id（源表 id）。
    int32_t id = 0;
    // 击飞类型：0 击退、1 击倒、2 击飞、-1 无受击。
    int32_t hitType = 0;
    // 地面 / 空中 / 倒地位移配置 id。
    int32_t displacementId = 0;
    int32_t airDisplacementId = 0;
    int32_t floorDisplacementId = 0;
    // 顿帧：时长及作用于攻击者 / 特效，受击者延迟帧。
    int32_t freezeTime = 0;
    int32_t freezeTimeControlEffect = 0;
    int32_t freezeTimeControlRole = 0;
    int32_t freezeTimeDelay = 0;
    // 命中条件（hitCondition，0 无 / -1 不打倒地等）。
    int32_t hitCondition = 0;
    // 命中次数限制（-1 无限）。
    int32_t hitCounts = 0;
    // 命中间隔（-1 按目标去重）。
    int32_t hitInterval = 0;
    // 必中标记（hitMust）。
    int32_t hitMust = 0;
    // 韧性修正。
    Fixed hitRigidity;
    // PVE / PVP 伤害倍率。
    Fixed hurtRate;
    Fixed pvpHurtRate;
    // 攻防类型（hurtType）。
    int32_t hurtType = 0;
    // 硬直时长（stiffTime）。
    int32_t stiffTime = 0;

    MG_REFLECT(HitDef,
               id,
               hitType,
               displacementId,
               airDisplacementId,
               floorDisplacementId,
               freezeTime,
               freezeTimeControlEffect,
               freezeTimeControlRole,
               freezeTimeDelay,
               hitCondition,
               hitCounts,
               hitInterval,
               hitMust,
               hitRigidity,
               hurtRate,
               pvpHurtRate,
               hurtType,
               stiffTime)
};
}  // namespace mugen::conf::battle
