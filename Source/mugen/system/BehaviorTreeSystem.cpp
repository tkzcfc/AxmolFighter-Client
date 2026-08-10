#include "BehaviorTreeSystem.h"

#include "mugen/Components.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/bt/RoleTreeBuilder.h"
#include "mugen/bt/SkillCastRules.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/bt/BTContext.h"

#include <algorithm>

NS_MG_BEGIN

BehaviorTreeSystem::BehaviorTreeSystem() {}
BehaviorTreeSystem::~BehaviorTreeSystem() {}

void BehaviorTreeSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, BehaviorTreeComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, BehaviorComponent);
}

void BehaviorTreeSystem::onEntityAdded(Entity* entity)
{
    auto* bt = MG_GET_COMPONENT(entity, BehaviorTreeComponent);
    if (bt && !bt->root)
        RoleTreeBuilder::attachToEntity(entity);

    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (behavior && behavior->statusTags == 0)
        behavior->statusTags =
            StateTag::kTagGrounded | StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
}

void BehaviorTreeSystem::fillContext(Entity* entity, BTContext& ctx)
{
    ctx.entity       = entity;
    ctx.ecs          = getECSManager();
    ctx.bt           = MG_GET_COMPONENT(entity, BehaviorTreeComponent);
    ctx.behavior     = MG_GET_COMPONENT(entity, BehaviorComponent);
    ctx.skillCast    = MG_GET_COMPONENT(entity, SkillCastComponent);
    ctx.hitReact     = MG_GET_COMPONENT(entity, HitReactComponent);
    ctx.avatar       = MG_GET_COMPONENT(entity, AvatarComponent);
    ctx.transform    = MG_GET_COMPONENT(entity, TransformComponent);
    ctx.attribute    = MG_GET_COMPONENT(entity, AttributeComponent);
    ctx.displacement = MG_GET_COMPONENT(entity, DisplacementComponent);
    ctx.buff         = MG_GET_COMPONENT(entity, BuffComponent);
    ctx.input        = MG_GET_COMPONENT(entity, InputComponent);
    ctx.physics      = MG_GET_COMPONENT(entity, PhysicsComponent);
    ctx.skillDeck    = MG_GET_COMPONENT(entity, SkillDeckComponent);
}

void BehaviorTreeSystem::tryCastFromInput(Entity* entity)
{
    auto* behavior  = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* cast      = MG_GET_COMPONENT(entity, SkillCastComponent);
    auto* input     = MG_GET_COMPONENT(entity, InputComponent);
    auto* deck      = MG_GET_COMPONENT(entity, SkillDeckComponent);
    auto* skillBar  = MG_GET_COMPONENT(entity, SkillBarComponent);
    auto* actorData = MG_GET_COMPONENT(entity, ActorDataComponent);
    if (!behavior || !cast || !input || !deck || deck->skills.empty())
        return;
    if ((behavior->statusTags & StateTag::kTagAttackAllowed) == 0)
        return;
    if (behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState))
        return;
    if (behavior->landLockMs > 0)
        return;
    if (behavior->staticRemainMs > 0)
        return;

    const int64_t nowMs = getECSManager()->getRunningTimeMs();

    // 搓招：推进 comboInputs 匹配；完整匹配则优先生效
    int32_t skillId   = 0;
    int32_t inputSlot = 0;
    int32_t step      = 0;

    if (actorData)
    {
        for (auto& inst : actorData->skills)
        {
            if (inst.comboInputs.empty())
                continue;
            if (inst.comboInputsMatchedCount > 0 && inst.comboWindowMs > 0 &&
                nowMs - static_cast<int64_t>(inst.lastComboInputMatchedTime) > inst.comboWindowMs)
            {
                inst.comboInputsMatchedCount = 0;
            }

            const int32_t need = static_cast<int32_t>(inst.comboInputs.size());
            if (inst.comboInputsMatchedCount < 0 || inst.comboInputsMatchedCount >= need)
                inst.comboInputsMatchedCount = 0;

            const int32_t expectSlot = inst.comboInputs[static_cast<size_t>(inst.comboInputsMatchedCount)];
            if (!bt_util::justPressed(input, expectSlot))
                continue;

            ++inst.comboInputsMatchedCount;
            inst.lastComboInputMatchedTime = static_cast<uint64_t>(nowMs);
            if (inst.comboInputsMatchedCount < need)
                continue;

            inst.comboInputsMatchedCount = 0;
            skillId                      = inst.skillAttackId;
            inputSlot                    = expectSlot;
            step                         = 0;
            if (skillBar)
            {
                for (const auto& slot : skillBar->skillSlots)
                {
                    bool inSlot = false;
                    for (int32_t idx : slot.skillIndexs)
                    {
                        if (idx >= 0 && idx < static_cast<int32_t>(actorData->skills.size()) &&
                            actorData->skills[static_cast<size_t>(idx)].skillAttackId == skillId)
                        {
                            inSlot = true;
                            break;
                        }
                    }
                    if (!inSlot)
                        continue;
                    int32_t s = 0;
                    SkillCastRules::resolveFightSkill(entity, slot.slotIndex, &s);
                    inputSlot = slot.slotIndex;
                    step      = s;
                    break;
                }
            }
            break;
        }
    }

    if (skillId <= 0 && skillBar)
    {
        for (const auto& slot : skillBar->skillSlots)
        {
            if (slot.slotIndex <= 0)
                continue;

            int32_t s        = 0;
            const int32_t id = SkillCastRules::resolveFightSkill(entity, slot.slotIndex, &s);
            if (id <= 0)
                continue;

            uint32_t flags = SlotTriggerFlag::kSlotTriggerPress;
            if (actorData)
            {
                for (const auto& inst : actorData->skills)
                {
                    if (inst.skillAttackId == id)
                    {
                        // 有搓招序列的技能不走普通槽触发
                        if (!inst.comboInputs.empty())
                        {
                            flags = SlotTriggerFlag::kSlotTriggerNone;
                            break;
                        }
                        flags = inst.slotTriggerFlags;
                        break;
                    }
                }
            }
            if (flags == SlotTriggerFlag::kSlotTriggerNone)
                continue;
            if (!bt_util::slotTriggered(input, slot.slotIndex, flags))
                continue;

            skillId   = id;
            inputSlot = slot.slotIndex;
            step      = s;
            break;
        }
    }

    const int32_t thrustId = cast->thrustSkillAttackId;
    const bool dashThrust =
        skillId <= 0 && (behavior->statusTags & StateTag::kTagDashState) != 0 && thrustId > 0 &&
        bt_util::justPressed(input, static_cast<int32_t>(INPUT_SLOT_0));

    if (dashThrust)
    {
        skillId   = thrustId;
        inputSlot = static_cast<int32_t>(INPUT_SLOT_0) + 1;
        step      = 0;
        if (skillBar)
        {
            for (const auto& slot : skillBar->skillSlots)
            {
                int32_t s = 0;
                if (SkillCastRules::resolveFightSkill(entity, slot.slotIndex, &s) == thrustId)
                {
                    inputSlot = slot.slotIndex;
                    step      = s;
                    break;
                }
            }
        }
    }
    else if (skillId <= 0 && cast->crazySkillAttackId > 0 &&
             bt_util::justPressed(input, static_cast<int32_t>(INPUT_SLOT_C)))
    {
        // E/C：爆气技（要求 EP 满，见 isAllowCast）
        skillId   = cast->crazySkillAttackId;
        inputSlot = static_cast<int32_t>(INPUT_SLOT_C);
        step      = 0;
    }
    else if (skillId <= 0 && cast->dodgeSkillAttackId > 0 &&
             bt_util::justPressed(input, static_cast<int32_t>(INPUT_SLOT_X)))
    {
        // F/X：闪避
        skillId   = cast->dodgeSkillAttackId;
        inputSlot = static_cast<int32_t>(INPUT_SLOT_X);
        step      = 0;
    }
    else if (skillId <= 0 && bt_util::justPressed(input, static_cast<int32_t>(INPUT_SLOT_0)))
    {
        skillId   = deck->skills.front().skillAttackId;
        inputSlot = static_cast<int32_t>(INPUT_SLOT_0);
        step      = 0;
    }

    if (skillId <= 0)
        return;

    if (!SkillCastRules::isAllowCast(entity, skillId, false))
    {
        SkillCastRules::queueInputBuffer(entity, skillId, inputSlot, step);
        return;
    }

    SkillCastRules::presetSkill(entity, skillId, inputSlot, step);
}

void BehaviorTreeSystem::update()
{
    const int32_t dtMs          = getECSManager()->getLastUpdateTimeMs();
    const int64_t runningTimeMs = getECSManager()->getRunningTimeMs();

    for (Entity* entity : entities)
    {
        // 顿帧中：跳过硬直倒计时 / 输入 / BT（整段冻结）
        const bool frozen = [&]() {
            if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
                return attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0;
            return false;
        }();

        processPendingHits(entity);
        if (!frozen)
        {
            tickHitRecovery(entity, dtMs);
            tickSkillCooldowns(entity, dtMs);
            updateAirborneTags(entity);
            // 怪物：注入 AI 移动意图到 Input，再跑双击跑/技能输入
            if (auto* identity = MG_GET_COMPONENT(entity, IdentityComponent))
            {
                if (identity->category == EntityCategory::kMonster)
                {
                    auto* aiComp = MG_GET_COMPONENT(entity, AIComponent);
                    auto* input  = MG_GET_COMPONENT(entity, InputComponent);
                    if (aiComp && input)
                    {
                        auto setMove = [&](int32_t slot, bool down) {
                            if (down)
                                MG_BIT_SET(input->keyDown, 1u << slot);
                            else
                                MG_BIT_REMOVE(input->keyDown, 1u << slot);
                        };
                        setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT), aiComp->moveDirX < 0);
                        setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT), aiComp->moveDirX > 0);
                        setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_UP), aiComp->moveDirY > 0);
                        setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN), aiComp->moveDirY < 0);
                        aiComp->moveDirX = 0;
                        aiComp->moveDirY = 0;
                    }
                }
            }
            updateDoubleTapRun(entity, runningTimeMs);
            if (auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent))
            {
                if (cast->crazyActive && cast->crazyRemainMs > 0)
                {
                    cast->crazyRemainMs = (std::max)(0, cast->crazyRemainMs - dtMs);
                    if (cast->crazyRemainMs <= 0)
                    {
                        cast->crazyActive = false;
                        cast->modeIndex   = 0;
                    }
                }
            }
            SkillCastRules::tickInputBuffer(entity, dtMs);
            tryCastFromInput(entity);
        }
        else
        {
            continue;
        }

        BTContext ctx;
        fillContext(entity, ctx);
        ctx.dtMs          = dtMs;
        ctx.runningTimeMs = runningTimeMs;

        auto* bt = ctx.bt;
        if (!bt)
            continue;
        if (!bt->root)
            RoleTreeBuilder::attachToEntity(entity);
        if (!bt->root)
            continue;

        bt->root->tick(ctx, dtMs);
    }
}

void BehaviorTreeSystem::processPendingHits(Entity* entity)
{
    auto* hitReact = MG_GET_COMPONENT(entity, HitReactComponent);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!hitReact || !behavior || hitReact->pendingHits.empty())
        return;

    if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
    {
        if (attr->currentAttribute.hp <= 0.0f)
        {
            hitReact->pendingHits.clear();
            return;
        }
    }

    auto bestIt = hitReact->pendingHits.begin();
    for (auto it = bestIt + 1; it != hitReact->pendingHits.end(); ++it)
    {
        if (static_cast<int8_t>(it->hitType) > static_cast<int8_t>(bestIt->hitType))
            bestIt = it;
    }
    const PendingHitInfo hit = *bestIt;
    hitReact->pendingHits.clear();

    // 已在受击中：允许 remix（连段受击），不再因严重度较低而丢弃
    const bool alreadyHit = (behavior->statusTags & StateTag::kTagHitState) != 0;

    hitReact->activeHitType      = hit.hitType;
    hitReact->activeTableHitType = hit.tableHitType;
    hitReact->activeHitstunMs    = hit.hitstunMs;

    behavior->statusTags |= StateTag::kTagHitState;
    behavior->statusTags &=
        ~(StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed | StateTag::kTagDashState);
    behavior->hitStunRemainingMs = std::max(0, hit.hitstunMs);
    behavior->downRemainMs       = 0;
    behavior->getUpRemainMs      = 0;
    behavior->hitSwitchRemainMs  = 0;

    // 首次受击：abort BT；remix 时不清树记忆也可，但需打断施法
    if (!alreadyHit)
    {
        if (auto* bt = MG_GET_COMPONENT(entity, BehaviorTreeComponent))
        {
            if (bt->root)
            {
                BTContext ctx;
                fillContext(entity, ctx);
                bt->root->onExit(ctx);
            }
            bt->currentActionId = 0;
            bt->actionElapsedMs = 0;
            bt->effectSpawnMask = 0;
            bt->animationEnd    = false;
            std::fill(bt->selectorMemory.begin(), bt->selectorMemory.end(), static_cast<int8_t>(-1));
        }
    }
    SkillCastRules::forceInterruptCast(entity);

    if (auto* attacker = getECSManager()->getEntity(hit.attackerId))
    {
        auto* attackerTf = MG_GET_COMPONENT(attacker, TransformComponent);
        auto* selfTf     = MG_GET_COMPONENT(entity, TransformComponent);
        if (attackerTf && selfTf)
        {
            selfTf->facingDirection = (attackerTf->position.x > selfTf->position.x) ? FacingDirection::kFacingRight
                                                                                    : FacingDirection::kFacingLeft;
        }
    }

    auto* physics = MG_GET_COMPONENT(entity, PhysicsComponent);
    auto* avatar  = MG_GET_COMPONENT(entity, AvatarComponent);

    const bool airborne = physics && !physics->onGround;
    const bool isLaunch = hit.hitType == HitType::kHitLaunch || hit.impulseZ > 0.0f;
    const bool isDown   = hit.hitType == HitType::kHitDown;

    if (physics)
    {
        physics->impulseVelocity.x = hit.impulseX;
        physics->impulseVelocity.z = hit.impulseZ;
        if (isLaunch && hit.impulseZ > 0.0f)
            physics->onGround = 0;
    }

    // 表 hit_type 0/1/2 → 受击枝：
    // 空中 + type>=1 → HitDown；击飞 → HitUp；直接倒地 → HitFloor；其余 → Stun(Hit)
    if (airborne && hit.tableHitType >= 1 && !isLaunch)
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kHitDown);
        behavior->statusTags |= StateTag::kTagAirborne;
        behavior->statusTags &= ~StateTag::kTagGrounded;
        if (avatar)
            avatar->play("hit", 1, false);
    }
    else if (isLaunch)
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kHitUp);
        behavior->statusTags |= StateTag::kTagAirborne;
        behavior->statusTags &= ~StateTag::kTagGrounded;
        if (avatar)
            avatar->play("hit", 1, false);
    }
    else if (isDown)
    {
        behavior->currentKind  = static_cast<int32_t>(BehaviorKind::kHitFloor);
        behavior->statusTags |= StateTag::kTagDownState;
        behavior->downRemainMs = bt_util::kDownMs;
        behavior->hitStunRemainingMs = 0;
        if (avatar)
            avatar->play("hit", 1, false);
    }
    else
    {
        // tableHitType 0（及地面轻击）：Stun；type>=1 无击飞位移时也先 Stun，硬直结束进 HitSwitch
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kStun);
        if (avatar)
            avatar->play("hit", 1, false);
    }
    behavior->currentBranchIndex = -1;
}

void BehaviorTreeSystem::tickHitRecovery(Entity* entity, int32_t dtMs)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* bt       = MG_GET_COMPONENT(entity, BehaviorTreeComponent);
    auto* physics  = MG_GET_COMPONENT(entity, PhysicsComponent);
    auto* avatar   = MG_GET_COMPONENT(entity, AvatarComponent);
    if (!behavior)
        return;

    if (behavior->landLockMs > 0)
        behavior->landLockMs = std::max(0, behavior->landLockMs - dtMs);

    if (behavior->staticRemainMs > 0)
    {
        behavior->staticRemainMs = std::max(0, behavior->staticRemainMs - dtMs);
        behavior->statusTags &= ~(StateTag::kTagMovable | StateTag::kTagAttackAllowed);
        if (behavior->staticRemainMs == 0)
        {
            behavior->statusTags |=
                StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
        }
    }
    if (bt && bt->staticResetRemainMs > 0)
        bt->staticResetRemainMs = std::max(0, bt->staticResetRemainMs - dtMs);

    // HitUp / HitDown 落地 → HitFloor
    if ((behavior->statusTags & StateTag::kTagHitState) && physics && physics->onGround &&
        (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kHitUp) ||
         behavior->currentKind == static_cast<int32_t>(BehaviorKind::kHitDown)))
    {
        behavior->currentKind        = static_cast<int32_t>(BehaviorKind::kHitFloor);
        behavior->statusTags |= StateTag::kTagDownState;
        behavior->statusTags &= ~(StateTag::kTagAirborne | StateTag::kTagFalling);
        behavior->statusTags |= StateTag::kTagGrounded;
        behavior->downRemainMs       = bt_util::kDownMs;
        behavior->hitStunRemainingMs = 0;
        behavior->currentBranchIndex = -1;
        bt_util::playBranchAnim(behavior, avatar);
        return;
    }

    // HitSwitch 过渡结束 → Idle
    if (behavior->hitSwitchRemainMs > 0)
    {
        behavior->hitSwitchRemainMs = std::max(0, behavior->hitSwitchRemainMs - dtMs);
        if (behavior->hitSwitchRemainMs == 0 &&
            behavior->currentKind == static_cast<int32_t>(BehaviorKind::kHitSwitch))
        {
            behavior->statusTags &= ~StateTag::kTagHitState;
            behavior->statusTags |=
                StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
            behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
            if (auto* hr = MG_GET_COMPONENT(entity, HitReactComponent))
            {
                hr->activeHitType      = HitType::kHitNone;
                hr->activeTableHitType = 0;
            }
            bt_util::invalidateBranchAndPlay(behavior, avatar);
        }
        return;
    }

    if (behavior->hitStunRemainingMs > 0)
    {
        behavior->hitStunRemainingMs = std::max(0, behavior->hitStunRemainingMs - dtMs);
        if (behavior->hitStunRemainingMs == 0 &&
            behavior->currentKind == static_cast<int32_t>(BehaviorKind::kStun))
        {
            // Stun 结束 → HitSwitch 过渡，再回 Idle
            behavior->currentKind       = static_cast<int32_t>(BehaviorKind::kHitSwitch);
            behavior->hitSwitchRemainMs = bt_util::kHitSwitchMs;
            behavior->currentBranchIndex = -1;
            bt_util::playBranchAnim(behavior, avatar);
        }
        return;
    }

    if (behavior->downRemainMs > 0)
    {
        behavior->downRemainMs = std::max(0, behavior->downRemainMs - dtMs);
        if (behavior->downRemainMs == 0)
        {
            behavior->currentKind        = static_cast<int32_t>(BehaviorKind::kGetUp);
            behavior->getUpRemainMs      = bt_util::kGetUpMs;
            behavior->currentBranchIndex = -1;
            bt_util::playBranchAnim(behavior, avatar);
        }
        return;
    }

    if (behavior->getUpRemainMs > 0)
    {
        behavior->getUpRemainMs = std::max(0, behavior->getUpRemainMs - dtMs);
        if (behavior->getUpRemainMs == 0)
        {
            behavior->statusTags &= ~(StateTag::kTagHitState | StateTag::kTagDownState);
            behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed |
                                    StateTag::kTagFacingAllowed | StateTag::kTagGrounded;
            behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
            if (auto* hr = MG_GET_COMPONENT(entity, HitReactComponent))
            {
                hr->activeHitType      = HitType::kHitNone;
                hr->activeTableHitType = 0;
            }
            bt_util::invalidateBranchAndPlay(behavior, avatar);
        }
    }
}

void BehaviorTreeSystem::tickSkillCooldowns(Entity* entity, int32_t dtMs)
{
    auto* deck = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!deck)
        return;
    for (auto& e : deck->skills)
    {
        if (e.coolDownMs <= 0)
            continue;
        e.coolDownMs = std::max(0, e.coolDownMs - dtMs);
        if (e.coolDownMs == 0)
        {
            // addSkillReleaseCount：回满一段；未满 max 则继续下一轮 CD
            if (e.releaseCount < e.releaseMax)
                e.releaseCount = std::min(e.releaseMax, e.releaseCount + 1);
            if (e.releaseCount < e.releaseMax && e.coolDownMaxMs > 0)
                e.coolDownMs = static_cast<int32_t>(static_cast<float>(e.coolDownMaxMs) * e.coldTimeScale);
        }
    }
}

void BehaviorTreeSystem::updateAirborneTags(Entity* entity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* physics  = MG_GET_COMPONENT(entity, PhysicsComponent);
    if (!behavior || !physics)
        return;
    if (behavior->statusTags & StateTag::kTagHitState)
        return;

    if (physics->onGround)
    {
        if (behavior->statusTags & StateTag::kTagAirborne)
        {
            behavior->statusTags &= ~(StateTag::kTagAirborne | StateTag::kTagFalling);
            behavior->statusTags |= StateTag::kTagGrounded;
            if (behavior->landLockMs <= 0)
                behavior->landLockMs = bt_util::kLandLockMs;
        }
    }
    else
    {
        behavior->statusTags |= StateTag::kTagAirborne;
        behavior->statusTags &= ~StateTag::kTagGrounded;
        if (physics->velocity.z + physics->impulseVelocity.z < 0.0f)
            behavior->statusTags |= StateTag::kTagFalling;
        else
            behavior->statusTags &= ~StateTag::kTagFalling;
    }
}

void BehaviorTreeSystem::updateDoubleTapRun(Entity* entity, int64_t runningTimeMs)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* input    = MG_GET_COMPONENT(entity, InputComponent);
    if (!behavior || !input)
        return;
    if (behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState))
    {
        behavior->statusTags &= ~StateTag::kTagDashState;
        return;
    }

    if (!behavior->clickToWalk)
    {
        if (bt_util::anyMoveKeyDown(input))
            behavior->statusTags |= StateTag::kTagDashState;
        else
            behavior->statusTags &= ~StateTag::kTagDashState;
        return;
    }

    if (bt_util::anyMoveJustPressed(input))
    {
        const int32_t q = bt_util::moveQuadrantFromInput(input);
        if (q != 0)
        {
            if (behavior->lastMovePressMs > 0 &&
                (runningTimeMs - behavior->lastMovePressMs) <= bt_util::kMoveDoubleTapMs &&
                bt_util::isSameSide(q, behavior->lastMoveQuadrant))
            {
                behavior->statusTags |= StateTag::kTagDashState;
                // 攻击中进入 Dash → 请求跑取消
                if (auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent))
                {
                    if (cast->activeSkillAttackId > 0)
                        SkillCastRules::requestRunCancel(entity);
                }
            }
            behavior->lastMoveQuadrant = q;
            behavior->lastMovePressMs  = runningTimeMs;
        }
    }

    if (!bt_util::anyMoveKeyDown(input))
        behavior->statusTags &= ~StateTag::kTagDashState;
}

NS_MG_END
