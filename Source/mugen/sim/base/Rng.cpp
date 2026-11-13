#include "mugen/sim/base/Rng.h"
#include "mugen/sim/World.h"

namespace mugen::sim::rng
{
namespace
{
// DrawPolicy 覆盖值。Choice / AI 抽样在策略下固定取 0（首选），与摘要头部 choice=first 一致。
Fixed policyValue(const DrawPolicy& policy, DrawKind kind)
{
    switch (kind)
    {
    case DrawKind::Dodge:
        return policy.m_dodge;
    case DrawKind::Crit:
        return policy.m_crit;
    case DrawKind::DamageFloat:
        return policy.m_float;
    case DrawKind::BuffProbability:
    case DrawKind::SkillAiProb:
    case DrawKind::AbilityProb:
        return policy.m_probability;
    case DrawKind::Choice:
    case DrawKind::AiTarget:
    case DrawKind::AiWander:
    case DrawKind::AiChance:
        return {};
    }
    return {};
}
}  // namespace

// 策略存在时直接返回覆盖值；否则消耗一次本场随机数。
Fixed draw(World& world, DrawKind kind)
{
    if (const DrawPolicy* policy = world.drawPolicy())
        return policyValue(*policy, kind);
    return Fixed::fromInt(world.ecs().singleton<RngState>().m_random.randomN(0, 100));
}

// 整数均匀抽样：策略存在时返回 min（首选），否则消耗一次本场随机数。
int32_t drawN(World& world, DrawKind kind, int32_t min, int32_t max)
{
    if (world.drawPolicy())
        return min;
    return world.ecs().singleton<RngState>().m_random.randomN(min, max);
}

// 定点均匀抽样：策略存在时返回 min，否则消耗一次本场随机数。
Fixed drawF(World& world, DrawKind kind, Fixed min, Fixed max)
{
    if (world.drawPolicy())
        return min;
    return world.ecs().singleton<RngState>().m_random.randomF(min, max);
}
}  // namespace mugen::sim::rng
