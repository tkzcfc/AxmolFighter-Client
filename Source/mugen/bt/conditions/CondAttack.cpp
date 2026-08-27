#include "mugen/bt/conditions/CondAttack.h"

#include "mugen/buff/BuffManager.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/Components.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/StdC.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/skill/Skill.h"
#include "mugen/skill/SkillManager.h"

NS_MG_BEGIN

CondRoleAttack::CondRoleAttack() {}

CondRoleAttack::~CondRoleAttack() {}

CondAttackSlot::CondAttackSlot() {}

CondAttackSlot::CondAttackSlot(int32_t slot) : slot(slot) {}

CondAttackSlot::~CondAttackSlot() {}

CondAttackStep::CondAttackStep() {}

CondAttackStep::CondAttackStep(int32_t slot, int32_t stepIndex) : slot(slot), stepIndex(stepIndex) {}

CondAttackStep::~CondAttackStep() {}

CondAttackPipe::CondAttackPipe() {}

CondAttackPipe::CondAttackPipe(int32_t slot, int32_t stepIndex, int32_t pipeIndex, int32_t modeIndex)
    : slot(slot), stepIndex(stepIndex), pipeIndex(pipeIndex), modeIndex(modeIndex)
{}

CondAttackPipe::~CondAttackPipe() {}

CondAttackSlotIndex::CondAttackSlotIndex() {}

CondAttackSlotIndex::CondAttackSlotIndex(int32_t slot, int32_t slotIndex) : slot(slot), slotIndex(slotIndex) {}

CondAttackSlotIndex::~CondAttackSlotIndex() {}

CondAttackMode::CondAttackMode() {}

CondAttackMode::CondAttackMode(int32_t slot, int32_t slotIndex, int32_t modeIndex)
    : slot(slot), slotIndex(slotIndex), modeIndex(modeIndex)
{}

CondAttackMode::~CondAttackMode() {}

CondAttackToward::CondAttackToward() {}

CondAttackToward::CondAttackToward(int32_t towardIndex) : towardIndex(towardIndex) {}

CondAttackToward::~CondAttackToward() {}

namespace
{
SkillManager* sm(BTContext& ctx)
{
    return SkillManager::of(ctx.entity);
}
}  // namespace

bool CondRoleAttack::check(BTContext& ctx)
{
    auto* behavior = MG_GET_COMPONENT(ctx.entity, BehaviorComponent);
    if (!behavior)
        return false;
    if (behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState))
        return false;
    if (auto* buffMgr = BuffManager::of(ctx.entity))
    {
        if (buffMgr->stunRef > 0)
            return false;
    }
    auto* mgr = sm(ctx);
    return mgr && mgr->activeSkillAttackId > 0;
}

void CondRoleAttack::onExit(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (mgr && mgr->activeSkillAttackId > 0 && mgr->pendingSkillAttackId <= 0)
        mgr->onSlotEnded(ctx.entity, mgr->activeInputSlot);
}

bool CondAttackSlot::check(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (!mgr)
        return false;
    return mgr->activeSkillAttackId > 0 && mgr->activeInputSlot == slot;
}

bool CondAttackSlot::onEnter(BTContext& /*ctx*/)
{
    return true;
}

void CondAttackSlot::onExit(BTContext& ctx)
{
    if (auto* mgr = sm(ctx))
        mgr->onSlotEnded(ctx.entity, slot);
}

void CondAttackSlot::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(slot);
}

bool CondAttackSlot::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return byteBuffer.getInt32(slot);
}

bool CondAttackStep::check(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (!mgr)
        return false;
    return mgr->activeInputSlot == slot && mgr->activeStepInSlot == stepIndex;
}

bool CondAttackStep::onEnter(BTContext& ctx)
{
    if (auto* mgr = sm(ctx))
        mgr->onStepBegan(ctx.entity);
    return true;
}

void CondAttackStep::onExit(BTContext& ctx)
{
    if (auto* mgr = sm(ctx))
        mgr->onStepEnded(ctx.entity);
}

void CondAttackStep::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(slot);
    byteBuffer.writeInt32(stepIndex);
}

bool CondAttackStep::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return byteBuffer.getInt32(slot) && byteBuffer.getInt32(stepIndex);
}

bool CondAttackPipe::check(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (!mgr)
        return false;
    return mgr->activeInputSlot == slot && mgr->activeStepInSlot == stepIndex && mgr->modeIndex == modeIndex &&
           mgr->pipeIndex == pipeIndex;
}

bool CondAttackPipe::onEnter(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (auto* buffMgr = BuffManager::of(ctx.entity))
    {
        if (mgr && mgr->lastCastSkillId > 0 && mgr->lastCastSkillId != mgr->activeSkillAttackId &&
            (mgr->lastCastSlot != mgr->activeInputSlot || mgr->lastCastSlotIndex != mgr->activeSlotIndex))
        {
            buffMgr->trigger(ctx.entity, BFEvent::BeforeNextSkill, nullptr, mgr->activeSkillAttackId);
        }
        buffMgr->trigger(ctx.entity, BFEvent::BeforeCastSkill, nullptr, mgr ? mgr->activeSkillAttackId : 0);
    }
    if (mgr)
    {
        mgr->castBegan(ctx.entity);
        mgr->lastCastSkillId   = mgr->activeSkillAttackId;
        mgr->lastCastSlot      = mgr->activeInputSlot;
        mgr->lastCastSlotIndex = mgr->activeSlotIndex;
    }
    return true;
}

void CondAttackPipe::onExit(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (auto* buffMgr = BuffManager::of(ctx.entity))
    {
        buffMgr->trigger(ctx.entity, BFEvent::AfterCastSkill, nullptr, mgr ? mgr->activeSkillAttackId : 0);
        Skill* sk = mgr ? mgr->currentSkill() : nullptr;
        if (sk && sk->nextSkillAttackId <= 0)
            buffMgr->trigger(ctx.entity, BFEvent::AfterNextSkill, nullptr, mgr->activeSkillAttackId);
    }
    if (mgr)
        mgr->castEnded(ctx.entity);
}

void CondAttackPipe::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(slot);
    byteBuffer.writeInt32(stepIndex);
    byteBuffer.writeInt32(pipeIndex);
    byteBuffer.writeInt32(modeIndex);
}

bool CondAttackPipe::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return byteBuffer.getInt32(slot) && byteBuffer.getInt32(stepIndex) && byteBuffer.getInt32(pipeIndex) &&
           byteBuffer.getInt32(modeIndex);
}

bool CondAttackSlotIndex::check(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (!mgr)
        return false;
    return mgr->activeSkillAttackId > 0 && mgr->activeInputSlot == slot && mgr->activeSlotIndex == slotIndex;
}

void CondAttackSlotIndex::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(slot);
    byteBuffer.writeInt32(slotIndex);
}

bool CondAttackSlotIndex::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return byteBuffer.getInt32(slot) && byteBuffer.getInt32(slotIndex);
}

bool CondAttackMode::check(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (!mgr)
        return false;
    return mgr->activeInputSlot == slot && mgr->activeSlotIndex == slotIndex && mgr->modeIndex == modeIndex;
}

void CondAttackMode::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(slot);
    byteBuffer.writeInt32(slotIndex);
    byteBuffer.writeInt32(modeIndex);
}

bool CondAttackMode::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return byteBuffer.getInt32(slot) && byteBuffer.getInt32(slotIndex) && byteBuffer.getInt32(modeIndex);
}

bool CondAttackToward::check(BTContext& ctx)
{
    auto* mgr = sm(ctx);
    if (!mgr || mgr->activeSkillAttackId <= 0)
        return false;
    return mgr->towardIndex == towardIndex;
}

void CondAttackToward::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(towardIndex);
}

bool CondAttackToward::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    return byteBuffer.getInt32(towardIndex);
}

NS_MG_END
