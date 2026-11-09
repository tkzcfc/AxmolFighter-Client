#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf::battle
{
// 角色静态数据，由 role.lua 降级（见 DESIGN.md §9.1）。
struct RoleDef
{
    // 配置行 id（源表 id）。
    int32_t id = 0;
    // AI 配置 id 列表（aiId）。
    std::vector<int32_t> aiId;
    // 属性倍率（attributeRate）。
    std::vector<Fixed> attributeRate;
    // 出生 Buff（buffIds）。
    std::vector<int32_t> buffIds;
    // 死亡位移与死亡特效。
    int32_t deathDisplacementId = 0;
    int32_t deathEffectId = 0;
    // 浮空值消耗（fatigue）。
    int32_t fatigue = 0;
    // 受击限制（hitCount）。
    int32_t hitCount = 0;
    // 受击位移覆盖（hitDisplacementId）。
    int32_t hitDisplacementId = 0;
    // 受击保护限制（hitRestrain）。
    std::vector<int32_t> hitRestrain;
    // 受击硬直覆盖（hitStiffTime，-1 表示使用命中配置）。
    int32_t hitStiffTime = 0;
    // KO 特效（koEffectId）。
    int32_t koEffectId = 0;
    // 怪物阵营（monsterCamps）。
    int32_t monsterCamps = 0;
    // 碰撞半径。
    int32_t radius = 0;
    // 出生相对位置。
    std::vector<int32_t> relativePosition;
    // Spine 资源与扩展资源（动作文件/spine）。
    int32_t resSpineId = 0;
    int32_t resSpineIdExt = 0;
    // 韧性（rigidity）。
    int32_t rigidity = 0;
    // 角色类型（roleType）。
    int32_t roleType = 0;
    // 角色类型标记（roleTypeSign）。
    std::vector<int32_t> roleTypeSign;
    // 音效与受击音效类型。
    std::vector<int32_t> soundId;
    int32_t soundType = 0;
    // 怒气变身时间（timeRage）。
    std::vector<std::vector<int32_t>> timeRage;
    // 移动速度。
    Fixed velocity;
    // 重量（weight）。
    Fixed weight;
    // 伤害数字偏移（hurtNumPos，命中接触点偏移，战斗逻辑）。
    std::vector<int32_t> hurtNumPos;
    // 跨房保留标记（isPassRoom，1 表示换房时保留角色）。
    int32_t isPassRoom = 0;

    MG_REFLECT(RoleDef,
               id,
               aiId,
               attributeRate,
               buffIds,
               deathDisplacementId,
               deathEffectId,
               fatigue,
               hitCount,
               hitDisplacementId,
               hitRestrain,
               hitStiffTime,
               koEffectId,
               monsterCamps,
               radius,
               relativePosition,
               resSpineId,
               resSpineIdExt,
               rigidity,
               roleType,
               roleTypeSign,
               soundId,
               soundType,
               timeRage,
               velocity,
               weight,
               hurtNumPos,
               isPassRoom)
};
}  // namespace mugen::conf::battle
