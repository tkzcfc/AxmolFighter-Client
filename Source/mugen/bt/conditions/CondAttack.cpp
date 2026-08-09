#include "mugen/bt/conditions/CondAttack.h"

#include "mugen/bt/SkillCastRules.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/Components.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

bool CondRoleAttack::check(BTContext& ctx)
{
    if (!ctx.behavior)
        return false;
    if (ctx.behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState))
        return false;
    const int32_t skillId =
        ctx.skillCast ? ctx.skillCast->activeSkillAttackId : 0;
    return skillId > 0;
}

void CondRoleAttack::onExit(BTContext& ctx)
{
    // Slot.exit 已清；此处兜底（受击打断等）
    if (ctx.skillCast && ctx.skillCast->activeSkillAttackId > 0 &&
        ctx.skillCast->pendingSkillAttackId <= 0)
    {
        SkillCastRules::onSlotEnded(ctx.entity, ctx.skillCast->activeInputSlot);
    }
}

bool CondAttackSlot::check(BTContext& ctx)
{
    if (!ctx.skillCast)
        return false;
    return ctx.skillCast->activeSkillAttackId > 0 && ctx.skillCast->activeInputSlot == slot;
}

void CondAttackSlot::onEnter(BTContext& /*ctx*/) {}

void CondAttackSlot::onExit(BTContext& ctx)
{
    SkillCastRules::onSlotEnded(ctx.entity, slot);
}

bool CondAttackStep::check(BTContext& ctx)
{
    if (!ctx.skillCast)
        return false;
    return ctx.skillCast->activeInputSlot == slot && ctx.skillCast->activeStepInSlot == stepIndex;
}

void CondAttackStep::onEnter(BTContext& ctx)
{
    SkillCastRules::onStepBegan(ctx.entity);
}

void CondAttackStep::onExit(BTContext& ctx)
{
    SkillCastRules::onStepEnded(ctx.entity);
}

bool CondAttackPipe::check(BTContext& ctx)
{
    if (!ctx.skillCast)
        return false;
    return ctx.skillCast->pipeIndex == pipeIndex;
}

void CondAttackPipe::onEnter(BTContext& ctx)
{
    SkillCastRules::castBegan(ctx.entity);
}

void CondAttackPipe::onExit(BTContext& ctx)
{
    SkillCastRules::castEnded(ctx.entity);
}

bool CondAttackToward::check(BTContext& ctx)
{
    if (!ctx.skillCast)
        return false;
    return ctx.skillCast->towardIndex == towardIndex;
}

NS_MG_END
