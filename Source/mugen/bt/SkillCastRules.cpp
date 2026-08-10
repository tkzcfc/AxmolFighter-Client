#include "mugen/bt/SkillCastRules.h"

#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/buff/BuffApi.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Entity.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace SkillCastRules
{

namespace
{
SkillDeckEntry* findDeckEntry(SkillDeckComponent* deck, int32_t skillId)
{
    if (!deck)
        return nullptr;
    for (auto& e : deck->skills)
    {
        if (e.skillAttackId == skillId)
            return &e;
    }
    return nullptr;
}

const SkillInstanceData* findSkillInstance(const ActorDataComponent* actorData, int32_t skillAttackId)
{
    if (!actorData)
        return nullptr;
    for (const auto& s : actorData->skills)
    {
        if (s.skillAttackId == skillAttackId)
            return &s;
    }
    return nullptr;
}

bool passesSkillActivationTags(Entity* entity, int32_t skillAttackId)
{
    auto* behavior  = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* actorData = MG_GET_COMPONENT(entity, ActorDataComponent);
    if (!behavior)
        return false;

    const SkillInstanceData* inst = findSkillInstance(actorData, skillAttackId);
    const uint32_t allow = inst ? inst->allowTags : (StateTag::kTagGrounded | StateTag::kTagAttackAllowed);
    const uint32_t deny  = inst ? inst->denyTags : StateTag::kTagHitState;

    if (allow && (behavior->statusTags & allow) != allow)
        return false;
    if (deny && (behavior->statusTags & deny) != 0)
        return false;
    return true;
}

void clearInputBuffer(SkillCastComponent* cast)
{
    if (!cast)
        return;
    cast->bufferSkillAttackId = 0;
    cast->bufferInputSlot     = 0;
    cast->bufferStepInSlot    = 0;
    cast->bufferRemainMs      = 0;
    cast->bufferReleaseTags   = 0;
}

bool canConsumeInputBuffer(Entity* entity)
{
    auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!cast || !behavior || cast->bufferSkillAttackId <= 0 || cast->bufferRemainMs <= 0)
        return false;
    if (cast->bufferReleaseTags && (behavior->statusTags & cast->bufferReleaseTags) != 0)
        return false;
    return passesSkillActivationTags(entity, cast->bufferSkillAttackId);
}

void clearActiveSkillFields(SkillCastComponent* cast, BehaviorComponent* behavior)
{
    if (cast)
    {
        cast->activeSkillAttackId  = 0;
        cast->pendingSkillAttackId = 0;
        cast->pendingInputSlot     = 0;
        cast->pendingStepInSlot    = 0;
        cast->interruptOpen        = false;
        cast->interruptExtraOpen   = false;
        cast->wantRunCancel        = false;
        cast->costPaid             = false;
        cast->costPaidPipeIndex    = -1;
        cast->towardIndex          = 1;
        cast->meleeHitSkillId      = 0;
        cast->meleeHitTargetIds.clear();
        cast->meleeHitCounts.clear();
        cast->meleeHitCooldowns.clear();
    }
    if (behavior)
    {
        if (!(behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState)))
        {
            behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed;
            if (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kAttack))
                behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
        }
    }
}

void setActiveSkill(Entity* entity,
                    int32_t skillId,
                    int32_t inputSlot,
                    int32_t stepInSlot,
                    bool closeWindows)
{
    auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!cast || !behavior || skillId <= 0)
        return;

    const auto* cfg = Config::getInstance()->getSkillAttackConfigById(skillId);
    cast->activeSkillAttackId  = skillId;
    cast->activeInputSlot      = inputSlot;
    cast->activeStepInSlot     = stepInSlot;
    cast->pendingSkillAttackId = 0;
    cast->pendingInputSlot     = 0;
    cast->pendingStepInSlot    = 0;
    cast->wantRunCancel        = false;
    cast->costPaid             = false;
    cast->costPaidPipeIndex    = -1;
    if (closeWindows)
    {
        cast->interruptOpen      = false;
        cast->interruptExtraOpen = false;
    }
    if (cfg)
    {
        cast->releaseCount = 0;
        if (auto* entry = findDeckEntry(MG_GET_COMPONENT(entity, SkillDeckComponent), skillId))
            cast->releaseCount = entry->releaseCount;
    }

    behavior->currentKind        = static_cast<int32_t>(BehaviorKind::kAttack);
    behavior->currentBranchIndex = -1;
    behavior->statusTags &= ~StateTag::kTagMovable;
}

SkillVector vectorFromFacingRelativeInput(const InputComponent* input, FacingDirection facing)
{
    if (!input)
        return SkillVector::Front;
    float vx = 0.0f, vy = 0.0f;
    if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)))
        vx -= 1.0f;
    if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
        vx += 1.0f;
    if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)))
        vy += 1.0f;
    if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN)))
        vy -= 1.0f;
    if (facing == FacingDirection::kFacingLeft)
        vx = -vx;
    if (vx == 0.0f && vy == 0.0f)
        return SkillVector::Front;

    const int32_t dx = (vx > 0.3f) ? 1 : ((vx < -0.3f) ? -1 : 0);
    const int32_t dy = (vy > 0.3f) ? 1 : ((vy < -0.3f) ? -1 : 0);
    if (dx > 0 && dy == 0)
        return SkillVector::Front;
    if (dx > 0 && dy > 0)
        return SkillVector::FrontUp;
    if (dx > 0 && dy < 0)
        return SkillVector::FrontDown;
    if (dx == 0 && dy > 0)
        return SkillVector::Up;
    if (dx == 0 && dy < 0)
        return SkillVector::Down;
    if (dx < 0 && dy == 0)
        return SkillVector::Back;
    if (dx < 0 && dy > 0)
        return SkillVector::BackUp;
    if (dx < 0 && dy < 0)
        return SkillVector::BackDown;
    return SkillVector::Front;
}

bool actionRowValid(const SkillAttackConfig* cfg, int32_t towardIndex1Based)
{
    if (!cfg || towardIndex1Based <= 0)
        return false;
    const size_t idx = static_cast<size_t>(towardIndex1Based - 1);
    if (idx < cfg->actionIds.size())
    {
        const auto& row = cfg->actionIds[idx];
        return !row.values.empty() && row.values.front() != -1;
    }
    if (towardIndex1Based == 1 && !cfg->primaryActionIds.empty())
        return cfg->primaryActionIds.front() != -1;
    return false;
}
}  // namespace

bool hasOrderControl(const SkillAttackConfig* cfg, int32_t controlType)
{
    if (!cfg)
        return false;
    for (int32_t v : cfg->sorderControlType)
    {
        if (v == controlType)
            return true;
    }
    return false;
}

bool isPriority(const SkillAttackConfig* nextCfg, const SkillAttackConfig* curCfg)
{
    if (!nextCfg)
        return false;
    const int32_t nextOrder = nextCfg->sorder;
    const int32_t curOrder  = curCfg ? curCfg->sorder : 0;
    // 任一方 sorder==-1 或 next >= cur
    if (nextOrder == -1 || curOrder == -1)
        return true;
    return nextOrder >= curOrder;
}

bool isSuperPriority(const SkillAttackConfig* nextCfg,
                     const SkillAttackConfig* curCfg,
                     bool interruptOpen,
                     bool interruptExtraOpen)
{
    // isSuperPriority(self=next, skillBase=cur)：
    // cur 带 IgnoreOrder_InterruptFrame 且普通窗开；或 next 带 Extra 忽略且至尊窗开
    if (interruptOpen && hasOrderControl(curCfg, kIgnoreOrderInterruptFrame))
        return true;
    if (interruptExtraOpen && hasOrderControl(nextCfg, kIgnoreOrderInterruptExtraFrame))
        return true;
    return false;
}

int32_t pipeMaxOf(const SkillAttackConfig* cfg)
{
    if (!cfg)
        return 1;
    return cfg->cdCount > 0 ? cfg->cdCount : 1;
}

void resetSkillPipe(SkillCastComponent* cast, int32_t pipeMax)
{
    if (!cast)
        return;
    if (pipeMax < 1)
        pipeMax = 1;
    cast->prePipeIndex = pipeMax + 1;
    cast->pipeIndex    = pipeMax;
}

void expectSkillPipe(SkillCastComponent* cast)
{
    if (!cast)
        return;
    cast->expectPipeIndex = cast->prePipeIndex - 1;
}

void selectSkillPipe(SkillCastComponent* cast)
{
    if (!cast)
        return;
    cast->prePipeIndex = cast->pipeIndex;
    cast->pipeIndex    = cast->expectPipeIndex;
}

int32_t dealWithDirection(Entity* entity, const SkillAttackConfig* skillCfg)
{
    if (!skillCfg)
        return 1;
    auto* input = MG_GET_COMPONENT(entity, InputComponent);
    auto* tf    = MG_GET_COMPONENT(entity, TransformComponent);
    const FacingDirection facing =
        tf ? tf->facingDirection : FacingDirection::kFacingRight;

    SkillVector vec = vectorFromFacingRelativeInput(input, facing);
    int32_t index   = static_cast<int32_t>(vec);
    // Back 系折叠：> Down → -5
    if (index > static_cast<int32_t>(SkillVector::Down))
        index -= 5;
    if (index < 1)
        index = 1;

    if (actionRowValid(skillCfg, index))
        return index;
    return 1;
}

bool isAllowCast(Entity* entity, int32_t skillAttackId, bool /*isAutoCast*/)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
    auto* attr     = MG_GET_COMPONENT(entity, AttributeComponent);
    auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!behavior || skillAttackId <= 0)
        return false;

    const auto* skillAtk = Config::getInstance()->getSkillAttackConfigById(skillAttackId);
    if (!skillAtk)
        return false;

    if (skillAtk->type == -1 && (behavior->statusTags & StateTag::kTagHitState))
        return false;

    auto* entry = findDeckEntry(deck, skillAttackId);
    // isCooling：次数用尽且 CD 中
    if (entry && entry->releaseCount <= 0 && entry->coolDownMs > 0)
        return false;
    if (entry && entry->releaseCount <= 0 && entry->releaseMax > 0)
        return false;

    if (!passesSkillActivationTags(entity, skillAttackId))
        return false;

    if (attr)
    {
        // 爆气技：需 EP 满（对齐 E 槽 isEnoughEp）
        if (cast && cast->crazySkillAttackId > 0 && skillAttackId == cast->crazySkillAttackId)
        {
            if (attr->ep < attr->epMax)
                return false;
        }

        // MP：表值 * (1 + 实体缩放-1 + 技能缩放-1 + 连段倍率-1)
        float mpCost = static_cast<float>(skillAtk->mp);
        if (mpCost > 0.0f)
        {
            float skillMpScale = entry ? entry->mpConsumeScale : 1.0f;
            float rate = attr->mpConsumeScale - 1.0f + skillMpScale - 1.0f;
            if (cast && cast->activeStepInSlot > 1)
                rate += kLinkSkillMpRate - 1.0f;
            mpCost *= (1.0f + rate);
            if (attr->currentAttribute.mp < mpCost)
                return false;
        }
        float epCost = static_cast<float>(skillAtk->ep);
        if (epCost > 0.0f)
        {
            epCost = epCost * attr->epConsumeScale + attr->epPlus;
            if (attr->ep < epCost)
                return false;
        }
        if (skillAtk->crystal > 0 && attr->crystal < skillAtk->crystal)
            return false;
    }
    return true;
}

bool castBegan(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* deck = MG_GET_COMPONENT(entity, SkillDeckComponent);
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!cast || cast->activeSkillAttackId <= 0)
        return false;

    // 幂等：同一 pipe 已付费则跳过
    if (cast->costPaid && cast->costPaidPipeIndex == cast->pipeIndex)
        return true;

    const auto* skillAtk = Config::getInstance()->getSkillAttackConfigById(cast->activeSkillAttackId);
    if (!skillAtk)
        return false;

    auto* entry = findDeckEntry(deck, cast->activeSkillAttackId);

    if (attr)
    {
        float mpCost = static_cast<float>(skillAtk->mp);
        if (mpCost > 0.0f)
        {
            float skillMpScale = entry ? entry->mpConsumeScale : 1.0f;
            float rate = attr->mpConsumeScale - 1.0f + skillMpScale - 1.0f;
            if (cast->activeStepInSlot > 1)
                rate += kLinkSkillMpRate - 1.0f;
            mpCost *= (1.0f + rate);
            attr->currentAttribute.mp -= mpCost;
            BuffApi::trigger(entity, BFEvent::UseTp, nullptr, cast->activeSkillAttackId, mpCost);
        }
        float epCost = static_cast<float>(skillAtk->ep);
        if (epCost > 0.0f)
        {
            epCost = epCost * attr->epConsumeScale + attr->epPlus;
            attr->ep = (std::max)(0.0f, attr->ep - epCost);
            BuffApi::trigger(entity, BFEvent::UseEp, nullptr, cast->activeSkillAttackId, epCost);
        }
        if (skillAtk->crystal > 0)
            attr->crystal = (std::max)(0, attr->crystal - skillAtk->crystal);
    }

    cast->towardIndex = dealWithDirection(entity, skillAtk);

    selectSkillPipe(cast);
    expectSkillPipe(cast);

    if (entry)
    {
        if (entry->releaseMax > 0)
            entry->releaseCount = (std::max)(0, entry->releaseCount - 1);
        cast->releaseCount = entry->releaseCount;

        // dealWithCoolDown：coldMax = 表cd * coldTimeScale；多充能且已在 CD 保留余量
        if (entry->coolDownMaxMs > 0)
        {
            const int32_t scaledMax =
                static_cast<int32_t>(static_cast<float>(entry->coolDownMaxMs) * entry->coldTimeScale);
            if (!(entry->coolDownMs > 0 && entry->releaseMax > 1))
                entry->coolDownMs = (std::max)(0, scaledMax);
            BuffApi::trigger(entity, BFEvent::SkillColdStart, nullptr, cast->activeSkillAttackId,
                             static_cast<float>(entry->coolDownMs));
        }
    }

    cast->costPaid          = true;
    cast->costPaidPipeIndex = cast->pipeIndex;

    // 爆气：开表技能进入 crazy 态（modeIndex=1，持续窗口可序列化）
    if (cast->crazySkillAttackId > 0 && cast->activeSkillAttackId == cast->crazySkillAttackId)
    {
        cast->crazyActive  = true;
        cast->crazyRemainMs = 8000;
        cast->modeIndex     = 1;
    }

    syncBehaviorMirror(entity);
    return true;
}

bool castEnded(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast)
        return false;

    // 仅当当前仍是同一技能上下文时尝试衔接预输入
    if (cast->pendingSkillAttackId > 0)
        return dealWithNextSkillBase(entity);
    return false;
}

bool dealWithNextSkillBase(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast || cast->pendingSkillAttackId <= 0)
        return false;

    const int32_t nextId   = cast->pendingSkillAttackId;
    const int32_t nextSlot = cast->pendingInputSlot > 0 ? cast->pendingInputSlot : cast->activeInputSlot;
    const int32_t nextStep = cast->pendingStepInSlot;
    const auto* nextCfg    = Config::getInstance()->getSkillAttackConfigById(nextId);
    if (!nextCfg || !isAllowCast(entity, nextId, false))
        return false;

    cast->towardIndex = dealWithDirection(entity, nextCfg);
    setActiveSkill(entity, nextId, nextSlot, nextStep, true);

    // 同技能多段充能：不 reset，直接 select 推进通道；新技能由 Step.enter reset
    const auto* curBefore = Config::getInstance()->getSkillAttackConfigById(nextId);
    selectSkillPipe(cast);
    cast->costPaid          = false;
    cast->costPaidPipeIndex = -1;
    cast->towardIndex       = dealWithDirection(entity, curBefore);
    syncBehaviorMirror(entity);
    return true;
}

bool presetSkill(Entity* entity, int32_t skillAttackId, int32_t inputSlot, int32_t stepInSlot)
{
    auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!cast || !behavior || skillAttackId <= 0)
        return false;
    if (!isAllowCast(entity, skillAttackId, false))
        return false;

    const auto* nextCfg = Config::getInstance()->getSkillAttackConfigById(skillAttackId);
    const auto* curCfg =
        cast->activeSkillAttackId > 0
            ? Config::getInstance()->getSkillAttackConfigById(cast->activeSkillAttackId)
            : nullptr;

    // 空闲：直接激活
    if (cast->activeSkillAttackId <= 0)
    {
        setActiveSkill(entity, skillAttackId, inputSlot, stepInSlot, true);
        const int32_t pmax = pipeMaxOf(nextCfg);
        resetSkillPipe(cast, pmax);
        expectSkillPipe(cast);
        cast->towardIndex = dealWithDirection(entity, nextCfg);
        syncBehaviorMirror(entity);
        return true;
    }

    const bool sameSlot = (inputSlot == cast->activeInputSlot);
    const bool winOpen  = cast->interruptOpen;

    // 窗开 + 优先级够 → 直接替换
    if (winOpen && isPriority(nextCfg, curCfg))
    {
        const int32_t prevId = cast->activeSkillAttackId;
        setActiveSkill(entity, skillAttackId, inputSlot, stepInSlot, true);
        if (!sameSlot || prevId != skillAttackId)
        {
            resetSkillPipe(cast, pipeMaxOf(nextCfg));
            expectSkillPipe(cast);
        }
        else
        {
            selectSkillPipe(cast);
        }
        cast->towardIndex = dealWithDirection(entity, nextCfg);
        syncBehaviorMirror(entity);
        return true;
    }

    // 否则写入预输入（闪避槽特殊跳过缓冲：本项目无 "F"，保留全部缓冲）
    cast->pendingSkillAttackId = skillAttackId;
    cast->pendingInputSlot     = inputSlot;
    cast->pendingStepInSlot    = stepInSlot;
    return true;
}

void queueInputBuffer(Entity* entity, int32_t skillAttackId, int32_t inputSlot, int32_t stepInSlot)
{
    auto* cast      = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* actorData = MG_GET_COMPONENT(entity, ActorDataComponent);
    if (!cast || skillAttackId <= 0)
        return;
    const SkillInstanceData* inst = findSkillInstance(actorData, skillAttackId);
    cast->bufferSkillAttackId = skillAttackId;
    cast->bufferInputSlot     = inputSlot;
    cast->bufferStepInSlot    = stepInSlot;
    cast->bufferRemainMs =
        inst && inst->inputBufferTimeoutMs > 0 ? inst->inputBufferTimeoutMs : 500;
    cast->bufferReleaseTags = inst ? inst->inputBufferReleaseTags : 0;
}

void tickInputBuffer(Entity* entity, int32_t dtMs)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast || cast->bufferSkillAttackId <= 0)
        return;
    if (dtMs > 0)
        cast->bufferRemainMs = (std::max)(0, cast->bufferRemainMs - dtMs);
    if (cast->bufferRemainMs <= 0)
    {
        clearInputBuffer(cast);
        return;
    }
    if (!canConsumeInputBuffer(entity))
        return;

    const int32_t skillId = cast->bufferSkillAttackId;
    const int32_t slot    = cast->bufferInputSlot;
    const int32_t step    = cast->bufferStepInSlot;
    clearInputBuffer(cast);
    presetSkill(entity, skillId, slot, step);
}

int32_t resolveFightSkill(Entity* entity, int32_t inputSlot, int32_t* outStep)
{
    auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
    auto* skillBar = MG_GET_COMPONENT(entity, SkillBarComponent);
    if (!deck || !skillBar || deck->skills.empty())
        return 0;

    size_t barIndex = static_cast<size_t>(-1);
    for (size_t i = 0; i < skillBar->skillSlots.size(); ++i)
    {
        if (skillBar->skillSlots[i].slotIndex == inputSlot)
        {
            barIndex = i;
            break;
        }
    }
    if (barIndex == static_cast<size_t>(-1) || barIndex >= deck->slotSkillIndices.size())
        return 0;

    const auto& indices = deck->slotSkillIndices[barIndex];
    if (indices.empty())
        return 0;

    int32_t step = 0;
    if (cast && cast->activeInputSlot == inputSlot && cast->activeSkillAttackId > 0)
    {
        // 多段充能：还有次数则不推进 Step（对齐 getFightSkill isNextSKill）
        auto* entry = findDeckEntry(deck, cast->activeSkillAttackId);
        const bool advanceStep =
            !(entry && entry->releaseMax > 1 && entry->releaseCount > 0);
        if (advanceStep)
            step = cast->activeStepInSlot + 1;
        else
            step = cast->activeStepInSlot;
    }
    if (step < 0 || step >= static_cast<int32_t>(indices.size()))
        step = 0;

    const int32_t deckIndex = indices[static_cast<size_t>(step)];
    if (deckIndex < 0 || deckIndex >= static_cast<int32_t>(deck->skills.size()))
        return 0;

    if (outStep)
        *outStep = step;
    return deck->skills[static_cast<size_t>(deckIndex)].skillAttackId;
}

bool canConsumePendingOnInterrupt(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast || cast->pendingSkillAttackId <= 0 || !cast->interruptOpen)
        return false;

    const bool sameSlot = cast->pendingInputSlot == cast->activeInputSlot ||
                          cast->pendingInputSlot == 0;
    if (sameSlot)
        return true;  // 同槽不比 sorder

    const auto* nextCfg =
        Config::getInstance()->getSkillAttackConfigById(cast->pendingSkillAttackId);
    const auto* curCfg =
        Config::getInstance()->getSkillAttackConfigById(cast->activeSkillAttackId);
    return isPriority(nextCfg, curCfg);
}

bool canConsumePendingOnExtraInterrupt(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast || cast->pendingSkillAttackId <= 0 || !cast->interruptExtraOpen)
        return false;

    const bool sameSlot = cast->pendingInputSlot == cast->activeInputSlot ||
                          cast->pendingInputSlot == 0;
    const auto* nextCfg =
        Config::getInstance()->getSkillAttackConfigById(cast->pendingSkillAttackId);
    const auto* curCfg =
        Config::getInstance()->getSkillAttackConfigById(cast->activeSkillAttackId);

    if (!sameSlot)
        return isSuperPriority(nextCfg, curCfg, cast->interruptOpen, cast->interruptExtraOpen);

    // 同槽：非同一技能且带 IgnoreOrder_Extra
    if (cast->pendingSkillAttackId == cast->activeSkillAttackId)
        return false;
    return hasOrderControl(nextCfg, kIgnoreOrderInterruptExtraFrame);
}

void syncBehaviorMirror(Entity* entity)
{
    auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!cast || !behavior)
        return;
    if (cast->activeSkillAttackId > 0)
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kAttack);
    else if (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kAttack) &&
             !(behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState)))
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
}

void onSlotEnded(Entity* entity, int32_t slot)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast || cast->activeInputSlot != slot)
        return;
    // 有预输入则先尝试衔接
    if (cast->pendingSkillAttackId > 0)
    {
        dealWithNextSkillBase(entity);
        return;
    }

    // KeepPress：槽结束时仍按住 → 同槽 next（resolveFightSkill 推进 step）
    auto* input     = MG_GET_COMPONENT(entity, InputComponent);
    auto* actorData = MG_GET_COMPONENT(entity, ActorDataComponent);
    const SkillInstanceData* inst = findSkillInstance(actorData, cast->activeSkillAttackId);
    const uint32_t flags =
        inst ? inst->slotTriggerFlags : SlotTriggerFlag::kSlotTriggerPress;
    if (input && input->isKeyDown(slot) &&
        (flags & SlotTriggerFlag::kSlotTriggerKeepPress) != 0)
    {
        int32_t step      = 0;
        const int32_t nextId = resolveFightSkill(entity, slot, &step);
        if (nextId > 0 && nextId != cast->activeSkillAttackId)
        {
            // 清当前再 preset，避免同技能卡住
            clearActiveSkillFields(cast, MG_GET_COMPONENT(entity, BehaviorComponent));
            presetSkill(entity, nextId, slot, step);
            return;
        }
        if (nextId > 0)
        {
            // 同技能多段充能：仍可再 preset
            clearActiveSkillFields(cast, MG_GET_COMPONENT(entity, BehaviorComponent));
            presetSkill(entity, nextId, slot, step);
            return;
        }
    }

    clearActiveSkillFields(cast, MG_GET_COMPONENT(entity, BehaviorComponent));
}

void onStepBegan(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast || cast->activeSkillAttackId <= 0)
        return;
    const auto* cfg = Config::getInstance()->getSkillAttackConfigById(cast->activeSkillAttackId);
    resetSkillPipe(cast, pipeMaxOf(cfg));
    expectSkillPipe(cast);
}

void onStepEnded(Entity* entity)
{
    onStepBegan(entity);  // Step.exit 同样 reset+expect
}

void forceInterruptCast(Entity* entity)
{
    clearActiveSkillFields(MG_GET_COMPONENT(entity, SkillCastComponent),
                           MG_GET_COMPONENT(entity, BehaviorComponent));
}

bool canRunCancel(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast || cast->activeSkillAttackId <= 0)
        return false;
    const auto* cfg = Config::getInstance()->getSkillAttackConfigById(cast->activeSkillAttackId);
    if (!cfg)
        return false;
    if (kRunSorder < 0 || kRunSorder >= cfg->sorder)
        return true;
    if (cast->interruptOpen && hasOrderControl(cfg, kIgnoreOrderInterruptFrame))
        return true;
    return false;
}

void requestRunCancel(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast || cast->activeSkillAttackId <= 0)
        return;
    if (canRunCancel(entity))
        cast->wantRunCancel = true;
}

bool dealWithRun(Entity* entity)
{
    auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* input    = MG_GET_COMPONENT(entity, InputComponent);
    if (!cast || cast->pendingSkillAttackId > 0)
        return false;

    const bool wantRun = cast->wantRunCancel ||
                         (behavior && (behavior->statusTags & StateTag::kTagDashState) != 0 &&
                          bt_util::anyMoveKeyDown(input));
    if (!wantRun || !canRunCancel(entity))
        return false;

    cast->wantRunCancel        = false;
    cast->activeSkillAttackId  = 0;
    cast->pendingSkillAttackId = 0;
    cast->interruptOpen        = false;
    cast->interruptExtraOpen   = false;
    syncBehaviorMirror(entity);
    if (behavior)
    {
        behavior->statusTags |=
            StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed |
            StateTag::kTagDashState;
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kDash);
    }
    return true;
}

}  // namespace SkillCastRules

NS_MG_END
