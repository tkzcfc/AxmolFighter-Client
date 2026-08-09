#include "mugen/bt/SkillCastRules.h"

#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Entity.h"

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
    // TODO(Phase2): slotTriggerFlags / comboWindowMs / inputBuffer* 尚未消费，仅 allow/denyTags 生效

    if (allow && (behavior->statusTags & allow) != allow)
        return false;
    if (deny && (behavior->statusTags & deny) != 0)
        return false;
    return true;
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
    // 对齐黑月：任一方 sorder==-1 或 next >= cur
    if (nextOrder == -1 || curOrder == -1)
        return true;
    return nextOrder >= curOrder;
}

bool isSuperPriority(const SkillAttackConfig* nextCfg,
                     const SkillAttackConfig* curCfg,
                     bool interruptOpen,
                     bool interruptExtraOpen)
{
    // 黑月 SkillBase:isSuperPriority(self=next, skillBase=cur)：
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
        if (skillAtk->mp > 0 && attr->currentAttribute.mp < static_cast<float>(skillAtk->mp))
            return false;
        if (skillAtk->ep > 0 && attr->ep < static_cast<float>(skillAtk->ep))
            return false;
        // crystal：一期无资源字段，仅表校验跳过扣减
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
        if (skillAtk->mp > 0)
            attr->currentAttribute.mp -= static_cast<float>(skillAtk->mp);
        if (skillAtk->ep > 0)
            attr->ep = (std::max)(0.0f, attr->ep - static_cast<float>(skillAtk->ep));
    }

    cast->towardIndex = dealWithDirection(entity, skillAtk);

    selectSkillPipe(cast);
    expectSkillPipe(cast);

    if (entry)
    {
        if (entry->releaseMax > 0)
            entry->releaseCount = (std::max)(0, entry->releaseCount - 1);
        cast->releaseCount = entry->releaseCount;

        // dealWithCoolDown：开计时；次数用尽后 isCooling 才拦
        if (entry->coolDownMaxMs > 0)
        {
            if (entry->releaseMax > 1)
            {
                // 多段：保持/启动 CD 计时（黑月累加余量简化为未在 CD 则开满）
                if (entry->coolDownMs <= 0)
                    entry->coolDownMs = entry->coolDownMaxMs;
            }
            else
            {
                entry->coolDownMs = entry->coolDownMaxMs;
            }
        }
    }

    cast->costPaid          = true;
    cast->costPaidPipeIndex = cast->pipeIndex;
    syncBehaviorMirror(entity);
    return true;
}

bool castEnded(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast)
        return false;

    // 对齐黑月：仅当当前仍是同一技能上下文时尝试衔接预输入
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
    auto* cast  = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* input = MG_GET_COMPONENT(entity, InputComponent);
    if (!cast || cast->activeInputSlot != slot)
        return;
    // 有预输入则先尝试衔接；否则清技能
    if (cast->pendingSkillAttackId > 0)
    {
        dealWithNextSkillBase(entity);
        return;
    }
    // B3：技能结束仍按住同槽 → 自动接 next_skill
    if (input && input->isKeyDown(slot) && cast->activeSkillAttackId > 0)
    {
        auto* deck = MG_GET_COMPONENT(entity, SkillDeckComponent);
        if (auto* entry = findDeckEntry(deck, cast->activeSkillAttackId))
        {
            if (entry->nextSkillAttackId > 0)
            {
                const int32_t nextId   = entry->nextSkillAttackId;
                const int32_t nextStep = cast->activeStepInSlot + 1;
                clearActiveSkillFields(cast, MG_GET_COMPONENT(entity, BehaviorComponent));
                presetSkill(entity, nextId, slot, nextStep);
                return;
            }
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
    onStepBegan(entity);  // 黑月 Step.exit 同样 reset+expect
}

void forceInterruptCast(Entity* entity)
{
    clearActiveSkillFields(MG_GET_COMPONENT(entity, SkillCastComponent),
                           MG_GET_COMPONENT(entity, BehaviorComponent));
}

}  // namespace SkillCastRules

NS_MG_END
