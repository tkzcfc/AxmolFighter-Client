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
}  // namespace mugen::sim::rng
