#pragma once
#include "mugen/core/math/Fixed.h"
#include <cstdint>

namespace mugen::sim
{
class World;
namespace rng
{
// 命中与触发的抽样种类。一次命中的顺序固定为 Dodge → 触发概率 → Crit → Float（R15）。
enum class DrawKind : uint8_t
{
    // 属性闪避，返回 0–100。
    Dodge = 0,
    // 暴击。
    Crit,
    // 伤害浮动，R15 里暴击之后的那一次抽样。
    DamageFloat,
    // Buff 准入概率。
    BuffProbability,
    // 技能 AI 概率。
    SkillAiProb,
    // 能力触发概率。
    AbilityProb,
    // 加权分支选择。
    Choice,
    // AI 重选目标。
    AiTarget,
    // AI 游走偏移。
    AiWander
};
// 唯一随机入口。设了 DrawPolicy 时用覆盖值且不消耗 RngState，否则从本场随机数抽 0–100。
Fixed draw(World& world, DrawKind kind);
}  // namespace rng
}  // namespace mugen::sim
