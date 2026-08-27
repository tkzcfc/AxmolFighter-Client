#include "BehaviorTreeSystem.h"

#include "mugen/Components.h"
#include "mugen/ai/AiAgent.h"
#include "mugen/bt/BtLocomotionUtils.h"
#include "mugen/bt/RoleTreeBuilder.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/bt/BehaviorTree.h"
#include "mugen/skill/SkillManager.h"

#include <algorithm>
#include <vector>
#include <vector>

NS_MG_BEGIN

namespace
{

// 有 SkillCastComponent 则 ensure；按卡组建 Skill 列表，再填快照
void prepareSkillManager(Entity* entity)
{
    auto* cast = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!cast)
        return;
    auto* mgr = cast->ensureManager();
    mgr->bindConfig(entity);
    cast->restoreRuntimeData();
}

bool isDeadHp(Entity* entity)
{
    return bt_util::isDeadHp(entity);
}

void interruptCast(Entity* entity)
{
    if (auto* mgr = SkillManager::of(entity))
        mgr->forceInterruptCast(entity);
}

BehaviorKind remixHitKind(int32_t prevKind, const PendingHitInfo& hit, BehaviorComponent* behavior)
{
    const auto prev  = static_cast<BehaviorKind>(prevKind);
    const bool rigid = bt_util::isRigidity(behavior);
    if (prev == BehaviorKind::kHitUp)
    {
        if (hit.tableHitType == 1)
            return BehaviorKind::kHitDown;
        return BehaviorKind::kHitUp;
    }
    if (prev == BehaviorKind::kHitDown)
    {
        if ((hit.tableHitType == 0 || hit.tableHitType == 2) && !rigid)
            return BehaviorKind::kHitUp;
        return BehaviorKind::kHitDown;
    }
    if (prev == BehaviorKind::kHitFloor)
    {
        if (hit.tableHitType == 1)
            return BehaviorKind::kHitDown;
        if ((hit.tableHitType == 0 || hit.tableHitType == 2) && !rigid)
            return BehaviorKind::kHitUp;
        return BehaviorKind::kHitFloor;
    }
    return BehaviorKind::kStun;
}

}  // namespace

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
    prepareSkillManager(entity);

    auto* bt = MG_GET_COMPONENT(entity, BehaviorTreeComponent);
    if (bt)
    {
        bt->restoreRuntimeData();
        auto* tree = bt->ensureTree();
        if (!tree->getRoot())
            RoleTreeBuilder::attachToEntity(entity);
        else
            RoleTreeBuilder::rebindAttackSelector(bt);
    }

    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (behavior && behavior->statusTags == 0)
        behavior->statusTags =
            StateTag::kTagGrounded | StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
}

void BehaviorTreeSystem::fillContext(Entity* entity, BTContext& ctx)
{
    ctx.entity = entity;
}

void BehaviorTreeSystem::tryCastFromInput(Entity* entity)
{
    auto* behavior  = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* mgr       = SkillManager::of(entity);
    auto* input     = MG_GET_COMPONENT(entity, InputComponent);
    auto* deck      = MG_GET_COMPONENT(entity, SkillDeckComponent);
    auto* skillBar  = MG_GET_COMPONENT(entity, SkillBarComponent);
    auto* actorData = MG_GET_COMPONENT(entity, ActorDataComponent);
    if (!behavior || !mgr || !input || !deck || deck->skills.empty())
        return;
    if ((behavior->statusTags & StateTag::kTagAttackAllowed) == 0)
        return;
    if (behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState))
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
                    mgr->resolveFightSkill(entity, slot.slotIndex, &s);
                    inputSlot = slot.slotIndex;
                    step      = s;
                    break;
                }
            }
            break;
        }
    }

    const int32_t thrustId = mgr->thrustSkillAttackId;
    if (skillId <= 0 && (behavior->statusTags & StateTag::kTagDashState) != 0 && thrustId > 0 &&
        bt_util::justPressed(input, static_cast<int32_t>(INPUT_SLOT_0)))
    {
        skillId   = thrustId;
        inputSlot = static_cast<int32_t>(INPUT_SLOT_Z);
        step      = 0;
    }

    if (skillId <= 0 && skillBar)
    {
        for (const auto& slot : skillBar->skillSlots)
        {
            if (slot.slotIndex <= 0)
                continue;

            int32_t s        = 0;
            const int32_t id = mgr->resolveFightSkill(entity, slot.slotIndex, &s);
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

    if (skillId <= 0 && mgr->crazySkillAttackId > 0 && bt_util::justPressed(input, static_cast<int32_t>(INPUT_SLOT_C)))
    {
        skillId   = mgr->crazySkillAttackId;
        inputSlot = static_cast<int32_t>(INPUT_SLOT_C);
        step      = 0;
    }
    else if (skillId <= 0 && mgr->dodgeSkillAttackId > 0 &&
             bt_util::justPressed(input, static_cast<int32_t>(INPUT_SLOT_X)))
    {
        skillId   = mgr->dodgeSkillAttackId;
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

    if (!mgr->isAllowCast(entity, skillId, false))
    {
        mgr->queueInputBuffer(entity, skillId, inputSlot, step);
        return;
    }

    mgr->presetSkill(entity, skillId, inputSlot, step);
}

void BehaviorTreeSystem::update()
{
    const int32_t dtMs          = getECSManager()->getLastUpdateTimeMs();
    const int64_t runningTimeMs = getECSManager()->getRunningTimeMs();
    std::vector<Entity*> toDestroy;
    // 施法会 spawn 特效并 notifyEntityReady，不能边遍历边改 entities
    const std::vector<Entity*> ticking = entities;

    for (Entity* entity : ticking)
    {
        if (!entity || entity->isPendingRemoval())
            continue;
        // 顿帧中：跳过硬直倒计时 / 输入 / BT（整段冻结）
        const bool frozen = [&]() {
            if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
                return attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0;
            return false;
        }();

        prepareSkillManager(entity);
        if (isDeadHp(entity))
            interruptCast(entity);
        processPendingHits(entity);
        if (!frozen)
        {
            tickHitRecovery(entity, dtMs);
            if (auto* mgr = SkillManager::of(entity))
                mgr->update(entity, dtMs);
            updateAirborneTags(entity);
            // 怪物：注入 AI 移动意图到 Input，再跑双击跑/技能输入
            if (auto* identity = MG_GET_COMPONENT(entity, IdentityComponent))
            {
                if (identity->category == EntityCategory::kMonster)
                {
                    auto* agent = AiAgent::of(entity);
                    auto* input = MG_GET_COMPONENT(entity, InputComponent);
                    if (agent && input)
                    {
                        auto setMove = [&](int32_t slot, bool down) {
                            if (down)
                                MG_BIT_SET(input->keyDown, 1u << slot);
                            else
                                MG_BIT_REMOVE(input->keyDown, 1u << slot);
                        };
                        setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT), agent->moveDirX < 0);
                        setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT), agent->moveDirX > 0);
                        setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_UP), agent->moveDirY > 0);
                        setMove(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN), agent->moveDirY < 0);
                        agent->moveDirX = 0;
                        agent->moveDirY = 0;
                    }
                }
            }
            updateDoubleTapRun(entity, runningTimeMs);
            tryCastFromInput(entity);
        }
        else
        {
            continue;
        }

        BTContext ctx;
        fillContext(entity, ctx);

        auto* bt = MG_GET_COMPONENT(entity, BehaviorTreeComponent);
        if (!bt)
            continue;
        auto* tree = bt->ensureTree();
        if (!tree->getRoot())
            RoleTreeBuilder::attachToEntity(entity);
        if (!tree->getRoot())
            continue;

        if (!tree->update(ctx, dtMs))
            tree->enter(ctx);

        if (auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent))
        {
            if (behavior->pendingDestroy)
            {
                behavior->pendingDestroy = false;
                toDestroy.push_back(entity);
            }
        }
    }
    for (Entity* e : toDestroy)
        getECSManager()->destroyEntity(e);
}

void BehaviorTreeSystem::processPendingHits(Entity* entity)
{
    auto* hitReact = MG_GET_COMPONENT(entity, HitReactComponent);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!hitReact || !behavior || hitReact->pendingHits.empty())
        return;

    const bool alreadyHit = (behavior->statusTags & StateTag::kTagHitState) != 0;

    if (isDeadHp(entity))
    {
        interruptCast(entity);
        if (!alreadyHit)
        {
            hitReact->pendingHits.clear();
            bt_util::enterDeath(entity);
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

    hitReact->activeHitType        = hit.hitType;
    hitReact->activeTableHitType   = hit.tableHitType;
    hitReact->activeHitstunMs      = hit.hitstunMs;
    hitReact->activeDisplacementId = hit.displacementId;
    hitReact->activeHitRigidity    = hit.hitRigidity;
    hitReact->knockbackFacing      = hit.knockbackFacing != 0.0f ? hit.knockbackFacing : 1.0f;

    behavior->statusTags |= StateTag::kTagHitState;
    behavior->statusTags &=
        ~(StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed | StateTag::kTagDashState);
    behavior->hitStunRemainingMs = std::max(0, hit.hitstunMs);

    if (!alreadyHit)
    {
        if (auto* bt = BehaviorTreeComponent::of(entity))
        {
            auto* tree = bt->ensureTree();
            if (tree->getRoot())
            {
                BTContext ctx;
                fillContext(entity, ctx);
                tree->exit(ctx);
            }
        }
    }
    interruptCast(entity);

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
    if (physics)
    {
        physics->impulseVelocity.x = hit.impulseX;
        physics->impulseVelocity.z = hit.impulseZ;
    }

    const bool airborne = physics && !physics->onGround;
    (void)airborne;
    if (!alreadyHit)
    {
        behavior->hitCounts = 1;
        bt_util::mixHitKind(entity, BehaviorKind::kStun, true);
        return;
    }

    if (bt_util::isRigidity(behavior))
        behavior->hitCounts += 1;

    const BehaviorKind next = remixHitKind(behavior->currentKind, hit, behavior);
    if (next != static_cast<BehaviorKind>(behavior->currentKind))
    {
        if (next == BehaviorKind::kHitUp || next == BehaviorKind::kHitDown || next == BehaviorKind::kHitFloor)
        {
            if (hit.tableHitType != 1)
                bt_util::dealWithWeight(entity, hit.hitRigidity);
        }
        bt_util::mixHitKind(entity, next, true);
    }
    else
        hitReact->doubleHitPending = true;
}

void BehaviorTreeSystem::tickHitRecovery(Entity* entity, int32_t dtMs)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
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
            behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
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

    const bool casting = [&]() {
        if ((behavior->statusTags & StateTag::kTagAttackState) != 0)
            return true;
        if (auto* mgr = SkillManager::of(entity))
            return mgr->activeSkillAttackId > 0;
        return false;
    }();

    // 攻击中走/跑状态加不上（当前必须恰好是 Idle）；只通知额外中断窗。
    // 已有的跑位保留，松杆也不在攻击中清掉。
    if (casting)
    {
        if (bt_util::anyMoveKeyDown(input))
        {
            if (auto* mgr = SkillManager::of(entity))
            {
                if (mgr->activeSkillAttackId > 0)
                    mgr->requestRunCancel(entity);
            }
        }
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
            }
            behavior->lastMoveQuadrant = q;
            behavior->lastMovePressMs  = runningTimeMs;
        }
    }

    if (!bt_util::anyMoveKeyDown(input))
        behavior->statusTags &= ~StateTag::kTagDashState;
}

NS_MG_END
