#include "mugen/skill/SkillManager.h"

#include "mugen/Components.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/buff/BFEvent.h"
#include "mugen/component/SkillCastComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/StdC.h"
#include "mugen/core/ecs/Entity.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace
{

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
    const uint32_t allow          = inst ? inst->allowTags : (StateTag::kTagGrounded | StateTag::kTagAttackAllowed);
    const uint32_t deny           = inst ? inst->denyTags : StateTag::kTagHitState;

    if (allow && (behavior->statusTags & allow) != allow)
        return false;
    if (deny && (behavior->statusTags & deny) != 0)
        return false;
    return true;
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
    return false;
}

bool deckIdsMatch(const SkillDeckComponent* deck, const std::vector<std::unique_ptr<Skill>>& skills)
{
    if (!deck || deck->skills.size() != skills.size())
        return false;
    for (size_t i = 0; i < skills.size(); ++i)
    {
        if (!skills[i] || skills[i]->skillAttackId != deck->skills[i].skillAttackId)
            return false;
    }
    return true;
}

}  // namespace

// 没有则创建空 manager；列表要等 bindConfig
SkillManager* SkillManager::of(Entity* entity)
{
    if (!entity)
        return nullptr;
    auto* comp = MG_GET_COMPONENT(entity, SkillCastComponent);
    return comp ? comp->ensureManager() : nullptr;
}

Skill* SkillManager::findSkill(int32_t skillAttackId) const
{
    if (skillAttackId <= 0)
        return nullptr;
    for (const auto& s : skills)
    {
        if (s && s->skillAttackId == skillAttackId)
            return s.get();
    }
    return nullptr;
}

void SkillManager::bindConfig(Entity* entity)
{
    auto* deck = entity ? MG_GET_COMPONENT(entity, SkillDeckComponent) : nullptr;
    if (!deck)
        return;
    if (deckIdsMatch(deck, skills))
        return;

    skills.clear();
    skills.reserve(deck->skills.size());
    for (const auto& e : deck->skills)
    {
        if (e.skillAttackId <= 0)
            continue;
        auto node = std::make_unique<Skill>();
        if (const auto* cfg = Config::getInstance()->getSkillAttackConfigById(e.skillAttackId))
            node->bindFromConfig(cfg);
        else
            node->skillAttackId = e.skillAttackId;
        node->nextSkillAttackId = e.nextSkillAttackId;
        node->level             = e.level;
        skills.push_back(std::move(node));
    }
}

void SkillManager::update(Entity* entity, int32_t dtMs)
{
    for (auto& s : skills)
    {
        if (!s)
            continue;
        const int32_t beforeCd  = s->coolDownMs;
        const int32_t beforeRel = s->releaseCount;
        s->tick(dtMs);
        if (beforeCd > 0 && (s->coolDownMs == 0 || s->releaseCount > beforeRel))
        {
            if (auto* buffMgr = BuffManager::of(entity))
                buffMgr->trigger(entity, BFEvent::SkillColdEnd, nullptr, s->skillAttackId);
        }
    }
    if (crazyActive)
    {
        auto* attr = entity ? MG_GET_COMPONENT(entity, AttributeComponent) : nullptr;
        if (attr && attr->epMax > 0.0f)
        {
            const float decay = static_cast<float>(dtMs) * kCrazyDecayPerMs * attr->epConsumeScale;
            attr->ep          = (std::max)(0.0f, attr->ep - decay);
            if (attr->ep <= 0.0f)
            {
                attr->ep = 0.0f;
                if (auto* buffMgr = BuffManager::of(entity))
                    buffMgr->trigger(entity, BFEvent::EpZero, nullptr, 0);
                // EP 到 0 不立即结束：等当前招结束，或切槽（pending）后再 endCrazy
                crazyEnding = true;
            }
        }
        if (crazyEnding && (activeSkillAttackId <= 0 || pendingSkillAttackId > 0))
            endCrazy(entity);
    }
    if (staticResetRemainMs > 0)
        staticResetRemainMs = (std::max)(0, staticResetRemainMs - dtMs);
    tickInputBuffer(entity, dtMs);
}

void SkillManager::clearInputBuffer()
{
    bufferSkillAttackId = 0;
    bufferInputSlot     = 0;
    bufferStepInSlot    = 0;
    bufferRemainMs      = 0;
    bufferReleaseTags   = 0;
}

void SkillManager::clearActiveSkillFields(Entity* entity)
{
    activeSkillAttackId  = 0;
    pendingSkillAttackId = 0;
    pendingInputSlot     = 0;
    pendingStepInSlot    = 0;
    activeInputSlot      = 0;
    activeStepInSlot     = 0;
    activeSlotIndex      = 1;
    interruptOpen        = false;
    interruptExtraOpen   = false;
    wantRunCancel        = false;
    costPaid             = false;
    costPaidPipeIndex    = -1;
    towardIndex          = 0;
    meleeHitSkillId      = 0;
    meleeHitTargetIds.clear();
    meleeHitCounts.clear();
    meleeHitCooldowns.clear();

    auto* behavior = entity ? MG_GET_COMPONENT(entity, BehaviorComponent) : nullptr;
    if (behavior)
    {
        behavior->statusTags &= ~StateTag::kTagAttackState;
        if (!(behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState)))
        {
            behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed;
            if (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kAttack))
                behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
        }
    }
}

void SkillManager::setActiveSkill(Entity* entity,
                                  int32_t skillId,
                                  int32_t inputSlot,
                                  int32_t stepInSlot,
                                  bool closeWindows)
{
    auto* behavior = entity ? MG_GET_COMPONENT(entity, BehaviorComponent) : nullptr;
    if (!behavior || skillId <= 0)
        return;

    activeSkillAttackId  = skillId;
    activeInputSlot      = inputSlot;
    activeStepInSlot     = stepInSlot;
    activeSlotIndex      = 1;
    pendingSkillAttackId = 0;
    pendingInputSlot     = 0;
    pendingStepInSlot    = 0;
    wantRunCancel        = false;
    costPaid             = false;
    costPaidPipeIndex    = -1;
    if (closeWindows)
    {
        modeIndex          = crazyActive ? 1 : 0;
        interruptOpen      = false;
        interruptExtraOpen = false;
    }

    behavior->currentKind        = static_cast<int32_t>(BehaviorKind::kAttack);
    behavior->currentBranchIndex = -1;
    behavior->statusTags &= ~StateTag::kTagMovable;
    behavior->statusTags |= StateTag::kTagAttackState;
}

bool SkillManager::hasOrderControl(const SkillAttackConfig* cfg, int32_t controlType)
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

bool SkillManager::isPriority(const SkillAttackConfig* nextCfg, const SkillAttackConfig* curCfg)
{
    if (!nextCfg)
        return false;
    const int32_t nextOrder = nextCfg->sorder;
    const int32_t curOrder  = curCfg ? curCfg->sorder : 0;
    if (nextOrder == -1 || curOrder == -1)
        return true;
    return nextOrder >= curOrder;
}

bool SkillManager::isSuperPriority(const SkillAttackConfig* nextCfg,
                                   const SkillAttackConfig* curCfg,
                                   bool interruptOpenFlag,
                                   bool interruptExtraOpenFlag)
{
    if (interruptOpenFlag && hasOrderControl(curCfg, kIgnoreOrderInterruptFrame))
        return true;
    if (interruptExtraOpenFlag && hasOrderControl(nextCfg, kIgnoreOrderInterruptExtraFrame))
        return true;
    return false;
}

int32_t SkillManager::pipeMaxOf(const SkillAttackConfig* cfg)
{
    if (!cfg)
        return 1;
    return cfg->cdCount > 0 ? cfg->cdCount : 1;
}

void SkillManager::resetSkillPipe(int32_t pipeMax)
{
    if (pipeMax < 1)
        pipeMax = 1;
    prePipeIndex = pipeMax + 1;
    pipeIndex    = pipeMax;
}

void SkillManager::expectSkillPipe()
{
    expectPipeIndex = prePipeIndex - 1;
}

void SkillManager::selectSkillPipe()
{
    prePipeIndex = pipeIndex;
    pipeIndex    = expectPipeIndex;
}

int32_t SkillManager::dealWithDirection(Entity* entity, const SkillAttackConfig* skillCfg)
{
    if (!skillCfg)
        return 1;
    auto* input                  = MG_GET_COMPONENT(entity, InputComponent);
    auto* tf                     = MG_GET_COMPONENT(entity, TransformComponent);
    const FacingDirection facing = tf ? tf->facingDirection : FacingDirection::kFacingRight;

    SkillVector vec = vectorFromFacingRelativeInput(input, facing);
    int32_t index   = static_cast<int32_t>(vec);
    if (index > static_cast<int32_t>(SkillVector::Down))
        index -= 5;
    if (index < 1)
        index = 1;

    if (actionRowValid(skillCfg, index))
        return index;
    return 1;
}

bool SkillManager::isAllowCast(Entity* entity, int32_t skillAttackId, bool /*isAutoCast*/)
{
    auto* behavior = entity ? MG_GET_COMPONENT(entity, BehaviorComponent) : nullptr;
    auto* attr     = entity ? MG_GET_COMPONENT(entity, AttributeComponent) : nullptr;
    if (!behavior || skillAttackId <= 0)
        return false;

    if (auto* buffMgr = BuffManager::of(entity))
    {
        if (buffMgr->stunRef > 0)
            return false;
    }

    const auto* skillAtk = Config::getInstance()->getSkillAttackConfigById(skillAttackId);
    if (!skillAtk)
        return false;

    if (skillAtk->type == -1 && (behavior->statusTags & StateTag::kTagHitState))
        return false;

    Skill* sk = findSkill(skillAttackId);
    if (sk && sk->releaseCount <= 0 && sk->coolDownMs > 0)
        return false;
    if (sk && sk->releaseCount <= 0 && sk->releaseMax > 0)
        return false;

    if (!passesSkillActivationTags(entity, skillAttackId))
        return false;

    if (attr)
    {
        const bool isCrazySkill = crazySkillAttackId > 0 && skillAttackId == crazySkillAttackId;

        float mpCost = static_cast<float>(skillAtk->mp);
        if (mpCost > 0.0f)
        {
            const float skillMpScale = sk ? sk->mpConsumeScale : 1.0f;
            mpCost *= skillMpScale * attr->mpConsumeScale;
            if (attr->mp < mpCost)
                return false;
        }

        const float epCost = static_cast<float>(skillAtk->ep) * (sk ? sk->epConsumeScale : 1.0f) * attr->epConsumeScale;
        if (crazyActive)
        {
            if (!isCrazySkill && epCost > 0.0f && attr->ep < epCost)
                return false;
        }
        else if (isCrazySkill)
        {
            if (attr->ep < attr->epMax)
                return false;
        }

        if (skillAtk->crystal > 0 && attr->crystal < skillAtk->crystal)
            return false;
    }
    return true;
}

bool SkillManager::castBegan(Entity* entity)
{
    if (activeSkillAttackId <= 0)
        return false;
    if (costPaid && costPaidPipeIndex == pipeIndex)
        return true;

    const auto* skillAtk = Config::getInstance()->getSkillAttackConfigById(activeSkillAttackId);
    if (!skillAtk)
        return false;

    Skill* sk  = currentSkill();
    auto* attr = entity ? MG_GET_COMPONENT(entity, AttributeComponent) : nullptr;

    if (attr)
    {
        if (!crazyActive)
        {
            float mpCost = static_cast<float>(skillAtk->mp);
            if (mpCost > 0.0f)
            {
                const float skillMpScale = sk ? sk->mpConsumeScale : 1.0f;
                mpCost *= skillMpScale * attr->mpConsumeScale;
                attr->mp -= mpCost;
                if (auto* buffMgr = BuffManager::of(entity))
                    buffMgr->trigger(entity, BFEvent::UseTp, nullptr, activeSkillAttackId, mpCost);
            }
            float epCost = static_cast<float>(skillAtk->ep);
            if (epCost > 0.0f)
            {
                epCost   = epCost * (sk ? sk->epConsumeScale : 1.0f) * attr->epConsumeScale;
                attr->ep = (std::max)(0.0f, attr->ep - epCost);
                if (auto* buffMgr = BuffManager::of(entity))
                    buffMgr->trigger(entity, BFEvent::UseEp, nullptr, activeSkillAttackId, epCost);
            }
        }
        if (skillAtk->crystal > 0)
            attr->crystal = (std::max)(0, attr->crystal - skillAtk->crystal);
    }

    towardIndex = dealWithDirection(entity, skillAtk);
    selectSkillPipe();
    expectSkillPipe();

    if (sk)
    {
        if (sk->releaseMax > 0)
            sk->releaseCount = (std::max)(0, sk->releaseCount - 1);
        if (sk->coolDownMaxMs > 0)
        {
            const int32_t scaledMax = static_cast<int32_t>(static_cast<float>(sk->coolDownMaxMs) * sk->coldTimeScale);
            if (!(sk->coolDownMs > 0 && sk->releaseMax > 1))
                sk->coolDownMs = (std::max)(0, scaledMax);
            if (auto* buffMgr = BuffManager::of(entity))
                buffMgr->trigger(entity, BFEvent::SkillColdStart, nullptr, activeSkillAttackId,
                                 static_cast<float>(sk->coolDownMs));
        }
    }

    costPaid          = true;
    costPaidPipeIndex = pipeIndex;

    if (crazySkillAttackId > 0 && activeSkillAttackId == crazySkillAttackId)
        startCrazy(entity);

    syncBehaviorMirror(entity);
    return true;
}

bool SkillManager::castEnded(Entity* entity)
{
    towardIndex = 0;
    if (pendingSkillAttackId <= 0)
        return false;

    const bool sameSlot = pendingInputSlot == 0 || pendingInputSlot == activeInputSlot;
    if (!sameSlot || pendingStepInSlot != activeStepInSlot)
        return false;

    selectSkillPipe();
    interruptOpen        = false;
    interruptExtraOpen   = false;
    costPaid             = false;
    costPaidPipeIndex    = -1;
    pendingSkillAttackId = 0;
    pendingInputSlot     = 0;
    pendingStepInSlot    = 0;
    syncBehaviorMirror(entity);
    return true;
}

bool SkillManager::dealWithNextSkillBase(Entity* entity)
{
    if (pendingSkillAttackId <= 0)
        return false;

    const int32_t nextId   = pendingSkillAttackId;
    const int32_t nextSlot = pendingInputSlot > 0 ? pendingInputSlot : activeInputSlot;
    const int32_t nextStep = pendingStepInSlot;
    const auto* nextCfg    = Config::getInstance()->getSkillAttackConfigById(nextId);
    if (!nextCfg || !isAllowCast(entity, nextId, false))
        return false;

    towardIndex = dealWithDirection(entity, nextCfg);
    setActiveSkill(entity, nextId, nextSlot, nextStep, true);
    selectSkillPipe();
    costPaid          = false;
    costPaidPipeIndex = -1;
    towardIndex       = dealWithDirection(entity, nextCfg);
    syncBehaviorMirror(entity);
    return true;
}

bool SkillManager::presetSkill(Entity* entity, int32_t skillAttackId, int32_t inputSlot, int32_t stepInSlot)
{
    auto* behavior = entity ? MG_GET_COMPONENT(entity, BehaviorComponent) : nullptr;
    if (!behavior || skillAttackId <= 0)
        return false;
    if (!isAllowCast(entity, skillAttackId, false))
        return false;

    const auto* nextCfg = Config::getInstance()->getSkillAttackConfigById(skillAttackId);
    const auto* curCfg =
        activeSkillAttackId > 0 ? Config::getInstance()->getSkillAttackConfigById(activeSkillAttackId) : nullptr;

    if (activeSkillAttackId <= 0)
    {
        setActiveSkill(entity, skillAttackId, inputSlot, stepInSlot, true);
        resetSkillPipe(pipeMaxOf(nextCfg));
        expectSkillPipe();
        towardIndex = dealWithDirection(entity, nextCfg);
        syncBehaviorMirror(entity);
        return true;
    }

    const bool sameSlot = (inputSlot == activeInputSlot);
    if (interruptOpen && isPriority(nextCfg, curCfg))
    {
        const int32_t prevId = activeSkillAttackId;
        setActiveSkill(entity, skillAttackId, inputSlot, stepInSlot, true);
        if (!sameSlot || prevId != skillAttackId)
        {
            resetSkillPipe(pipeMaxOf(nextCfg));
            expectSkillPipe();
        }
        else
        {
            selectSkillPipe();
        }
        towardIndex = dealWithDirection(entity, nextCfg);
        syncBehaviorMirror(entity);
        return true;
    }

    pendingSkillAttackId = skillAttackId;
    pendingInputSlot     = inputSlot;
    pendingStepInSlot    = stepInSlot;
    return true;
}

void SkillManager::queueInputBuffer(Entity* entity, int32_t skillAttackId, int32_t inputSlot, int32_t stepInSlot)
{
    auto* actorData = entity ? MG_GET_COMPONENT(entity, ActorDataComponent) : nullptr;
    if (skillAttackId <= 0)
        return;
    const SkillInstanceData* inst = findSkillInstance(actorData, skillAttackId);
    bufferSkillAttackId           = skillAttackId;
    bufferInputSlot               = inputSlot;
    bufferStepInSlot              = stepInSlot;
    bufferRemainMs                = inst && inst->inputBufferTimeoutMs > 0 ? inst->inputBufferTimeoutMs : 500;
    bufferReleaseTags             = inst ? inst->inputBufferReleaseTags : 0;
}

void SkillManager::tickInputBuffer(Entity* entity, int32_t dtMs)
{
    if (bufferSkillAttackId <= 0)
        return;
    if (dtMs > 0)
        bufferRemainMs = (std::max)(0, bufferRemainMs - dtMs);
    if (bufferRemainMs <= 0)
    {
        clearInputBuffer();
        return;
    }
    auto* behavior = entity ? MG_GET_COMPONENT(entity, BehaviorComponent) : nullptr;
    if (!behavior)
        return;
    if (bufferReleaseTags && (behavior->statusTags & bufferReleaseTags) != 0)
        return;
    if (!passesSkillActivationTags(entity, bufferSkillAttackId))
        return;

    const int32_t skillId = bufferSkillAttackId;
    const int32_t slot    = bufferInputSlot;
    const int32_t step    = bufferStepInSlot;
    clearInputBuffer();
    presetSkill(entity, skillId, slot, step);
}

int32_t SkillManager::resolveFightSkill(Entity* entity, int32_t inputSlot, int32_t* outStep)
{
    auto* deck     = entity ? MG_GET_COMPONENT(entity, SkillDeckComponent) : nullptr;
    auto* skillBar = entity ? MG_GET_COMPONENT(entity, SkillBarComponent) : nullptr;
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
    if (activeInputSlot == inputSlot && activeSkillAttackId > 0)
    {
        Skill* sk              = currentSkill();
        const bool advanceStep = !(sk && sk->releaseMax > 1 && sk->releaseCount > 0);
        step                   = advanceStep ? activeStepInSlot + 1 : activeStepInSlot;
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

int32_t SkillManager::findSlotForSkill(Entity* entity, int32_t skillAttackId, int32_t* outStep)
{
    if (outStep)
        *outStep = 0;
    if (dodgeSkillAttackId > 0 && skillAttackId == dodgeSkillAttackId)
        return static_cast<int32_t>(INPUT_SLOT_X);
    if (crazySkillAttackId > 0 && skillAttackId == crazySkillAttackId)
        return static_cast<int32_t>(INPUT_SLOT_C);
    if (thrustSkillAttackId > 0 && skillAttackId == thrustSkillAttackId)
        return static_cast<int32_t>(INPUT_SLOT_Z);

    auto* deck     = entity ? MG_GET_COMPONENT(entity, SkillDeckComponent) : nullptr;
    auto* skillBar = entity ? MG_GET_COMPONENT(entity, SkillBarComponent) : nullptr;
    if (!deck || !skillBar)
        return 0;

    for (size_t i = 0; i < skillBar->skillSlots.size(); ++i)
    {
        if (i >= deck->slotSkillIndices.size())
            break;
        const auto& indices = deck->slotSkillIndices[i];
        for (size_t step = 0; step < indices.size(); ++step)
        {
            const int32_t di = indices[step];
            if (di < 0 || di >= static_cast<int32_t>(deck->skills.size()))
                continue;
            if (deck->skills[static_cast<size_t>(di)].skillAttackId != skillAttackId)
                continue;
            if (outStep)
                *outStep = static_cast<int32_t>(step);
            return skillBar->skillSlots[i].slotIndex;
        }
    }
    return 0;
}

bool SkillManager::canConsumePendingOnInterrupt(Entity* /*entity*/)
{
    if (pendingSkillAttackId <= 0 || !interruptOpen)
        return false;
    const bool sameSlot = pendingInputSlot == activeInputSlot || pendingInputSlot == 0;
    if (sameSlot)
        return true;
    const auto* nextCfg = Config::getInstance()->getSkillAttackConfigById(pendingSkillAttackId);
    const auto* curCfg  = Config::getInstance()->getSkillAttackConfigById(activeSkillAttackId);
    return isPriority(nextCfg, curCfg);
}

bool SkillManager::canConsumePendingOnExtraInterrupt(Entity* /*entity*/)
{
    if (pendingSkillAttackId <= 0 || !interruptExtraOpen)
        return false;
    const bool sameSlot = pendingInputSlot == activeInputSlot || pendingInputSlot == 0;
    const auto* nextCfg = Config::getInstance()->getSkillAttackConfigById(pendingSkillAttackId);
    const auto* curCfg  = Config::getInstance()->getSkillAttackConfigById(activeSkillAttackId);
    if (!sameSlot)
        return isSuperPriority(nextCfg, curCfg, interruptOpen, interruptExtraOpen);
    if (pendingSkillAttackId == activeSkillAttackId)
        return false;
    if (hasOrderControl(nextCfg, kIgnoreOrderInterruptExtraFrame))
        return true;
    return interruptOpen && hasOrderControl(curCfg, kIgnoreOrderInterruptFrame);
}

void SkillManager::syncBehaviorMirror(Entity* entity)
{
    auto* behavior = entity ? MG_GET_COMPONENT(entity, BehaviorComponent) : nullptr;
    if (!behavior)
        return;
    if (activeSkillAttackId > 0)
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kAttack);
    else if (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kAttack) &&
             !(behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState)))
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
}

void SkillManager::onSlotEnded(Entity* entity, int32_t slot)
{
    if (activeInputSlot != slot)
        return;
    if (pendingSkillAttackId > 0)
    {
        dealWithNextSkillBase(entity);
        return;
    }

    auto* input                   = entity ? MG_GET_COMPONENT(entity, InputComponent) : nullptr;
    auto* actorData               = entity ? MG_GET_COMPONENT(entity, ActorDataComponent) : nullptr;
    const SkillInstanceData* inst = findSkillInstance(actorData, activeSkillAttackId);
    const uint32_t flags          = inst ? inst->slotTriggerFlags : SlotTriggerFlag::kSlotTriggerPress;
    if (input && input->isKeyDown(slot) && (flags & SlotTriggerFlag::kSlotTriggerKeepPress) != 0)
    {
        int32_t step         = 0;
        const int32_t nextId = resolveFightSkill(entity, slot, &step);
        if (nextId > 0)
        {
            clearActiveSkillFields(entity);
            presetSkill(entity, nextId, slot, step);
            return;
        }
    }

    clearActiveSkillFields(entity);
}

void SkillManager::onStepBegan(Entity* entity)
{
    if (activeSkillAttackId <= 0)
        return;
    const auto* cfg = Config::getInstance()->getSkillAttackConfigById(activeSkillAttackId);
    resetSkillPipe(pipeMaxOf(cfg));
    expectSkillPipe();
    (void)entity;
}

void SkillManager::onStepEnded(Entity* entity)
{
    onStepBegan(entity);
}

void SkillManager::forceInterruptCast(Entity* entity)
{
    clearActiveSkillFields(entity);
}

void SkillManager::startCrazy(Entity* entity)
{
    const bool was = crazyActive;
    crazyActive    = true;
    crazyRemainMs  = 0;
    crazyEnding    = false;
    // 不翻转正在施法的 modeIndex；modeIndex 在 setActiveSkill 时按 crazyActive 推导，
    // 当前招在 Mode 0 就让它跑完 Mode 0，下一招才进 Mode 1（对照参照：crazy 是实体 flag，不打断当前招）
    if (activeSkillAttackId <= 0)
        modeIndex = 1;
    if (!was)
    {
        if (auto* buffMgr = BuffManager::of(entity))
            buffMgr->trigger(entity, BFEvent::BeforeCrazy, nullptr, 0);
    }
}

void SkillManager::endCrazy(Entity* entity)
{
    if (!crazyActive && crazyRemainMs <= 0)
        return;
    crazyActive   = false;
    crazyRemainMs = 0;
    crazyEnding   = false;
    // 不翻转正在施法的 modeIndex；下一招 setActiveSkill 按 crazyActive 推导
    if (activeSkillAttackId <= 0)
        modeIndex = 0;
    if (auto* buffMgr = BuffManager::of(entity))
        buffMgr->trigger(entity, BFEvent::AfterCrazy, nullptr, 0);
}

bool SkillManager::canRunCancel(Entity* /*entity*/)
{
    if (activeSkillAttackId <= 0)
        return false;
    const auto* cfg = Config::getInstance()->getSkillAttackConfigById(activeSkillAttackId);
    if (!cfg)
        return false;
    if (kRunSorder < 0 || kRunSorder >= cfg->sorder)
        return true;
    if (interruptOpen && hasOrderControl(cfg, kIgnoreOrderInterruptFrame))
        return true;
    return false;
}

void SkillManager::requestRunCancel(Entity* entity)
{
    if (activeSkillAttackId <= 0)
        return;
    if (!canRunCancel(entity))
        return;
    wantRunCancel        = true;
    pendingSkillAttackId = 0;
    pendingInputSlot     = 0;
    pendingStepInSlot    = 0;
}

bool SkillManager::dealWithRun(Entity* entity)
{
    auto* behavior = entity ? MG_GET_COMPONENT(entity, BehaviorComponent) : nullptr;
    auto* input    = entity ? MG_GET_COMPONENT(entity, InputComponent) : nullptr;

    const bool wantRun = wantRunCancel || (behavior && (behavior->statusTags & StateTag::kTagDashState) != 0 &&
                                           bt_util::anyMoveKeyDown(input));
    if (!wantRun || !canRunCancel(entity))
        return false;

    // 额外中断窗的跑取消只结束当前动作；丢掉预输入，当前技能仍在，
    // 剩余 Toward 动作继续跑完，攻击枝退出时再清技能
    wantRunCancel        = false;
    pendingSkillAttackId = 0;
    pendingInputSlot     = 0;
    pendingStepInSlot    = 0;
    return true;
}

void SkillManager::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeUint16(static_cast<uint16_t>(skills.size()));
    for (const auto& s : skills)
    {
        MG_ASSERT(s && "SkillManager: null Skill");
        s->serialize(byteBuffer);
    }
}

bool SkillManager::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    uint16_t count = 0;
    if (!byteBuffer.getUint16(count))
        return false;
    if (count != static_cast<uint16_t>(skills.size()))
    {
        MG_LOG_E("SkillManager: skill count mismatch expect={} got={}", skills.size(), count);
        MG_ASSERT(false);
        return false;
    }
    for (auto& s : skills)
    {
        MG_ASSERT(s && "SkillManager: null Skill");
        const int32_t expectId = s->skillAttackId;
        if (!s->deserialize(byteBuffer))
            return false;
        if (s->skillAttackId != expectId)
        {
            MG_LOG_E("SkillManager: skill id mismatch expect={} got={}", expectId, s->skillAttackId);
            MG_ASSERT(false);
            return false;
        }
    }
    return true;
}

NS_MG_END
