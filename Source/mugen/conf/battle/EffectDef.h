#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf::battle
{
// 战斗特效实体配置，由 effect.lua 降级（见 DESIGN.md §6.8）。
struct EffectDef
{
    // 配置行 id（源表 id）。
    int32_t id = 0;
    // 动作片段 id 列表（actionIds，指向 battle_effect_clip）。
    std::vector<int32_t> clips;
    // 放置：位置类型与相对位置。
    int32_t positionType = 0;
    std::vector<Fixed> relativePosition;
    // 生成时的方向（effectOriebtationX / Z）。
    int32_t orientationX = 0;
    int32_t orientationZ = 0;
    // 跟随：0 不跟随、1 跟随主人、2 跟随目标。
    int32_t follow = 0;
    // 寿命（autoRelease）：0 片段退出销毁、1 片段放完销毁、2/3 命中目标数达标销毁（3 触发后继）。
    int32_t autoRelease = 0;
    // 命中配置 id（hitId，指向 battle_hit）。
    int32_t hitId = 0;
    // 命中目标与命中附加控制。
    int32_t hitTarget = 0;
    std::vector<int32_t> hitExtraControl;
    // 命中能量与连击经验。
    Fixed energy;
    Fixed comboExp;
    // 命中附带 Buff：打到自己 / 打到队友 / 给目标。
    std::vector<int32_t> buffId;
    std::variant<int32_t, std::vector<int32_t>> buffAllId;
    std::vector<int32_t> debuffId;
    // 命中附带的特殊能力与受击特效。
    int32_t specialabilityId = 0;
    int32_t specialabilityAllId = 0;
    std::vector<int32_t> hitEffectIds;
    // 后继特效（nextEffectId）。
    int32_t nextEffectId = 0;
    // 可控特效标记（control，首批范围外）。
    int32_t control = 0;
    // 碰撞、半径、速度、类型与 Spine 资源。
    int32_t collision = 0;
    int32_t radius = 0;
    int32_t velocity = 0;
    int32_t effectType = 0;
    int32_t resSpineId = 0;

    MG_REFLECT(EffectDef,
               id,
               clips,
               positionType,
               relativePosition,
               orientationX,
               orientationZ,
               follow,
               autoRelease,
               hitId,
               hitTarget,
               hitExtraControl,
               energy,
               comboExp,
               buffId,
               buffAllId,
               debuffId,
               specialabilityId,
               specialabilityAllId,
               hitEffectIds,
               nextEffectId,
               control,
               collision,
               radius,
               velocity,
               effectType,
               resSpineId)
};
}  // namespace mugen::conf::battle
