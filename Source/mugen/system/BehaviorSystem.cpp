#include "BehaviorSystem.h"

#include "mugen/Components.h"
#include "mugen/combat/ActionRunner.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

NS_MG_BEGIN

namespace
{
std::unordered_map<Entity*, ActionRunner> s_runners;

constexpr int32_t kMoveDoubleTapMs = 400;  // 对齐黑月 Const.MOVE_INTERVAL=0.4s
constexpr float kRunRate           = 1.6f;
constexpr float kAirControl        = 0.8f;
constexpr float kDefaultJumpSpeed  = 600.0f;
constexpr int32_t kLandLockMs      = 80;
constexpr int32_t kDownMs          = 400;
constexpr int32_t kGetUpMs         = 350;
// 对齐黑月 SkillOrderControlType
constexpr int32_t kIgnoreOrderInterruptFrame      = 1;
constexpr int32_t kIgnoreOrderInterruptExtraFrame = 2;

bool justPressed(const InputComponent* input, int32_t slot)
{
    return input && input->isKeyDown(slot) && input->queryKeyPressedDurationMs(slot) == 0;
}

// 象限：1右 2上 3左 4下；斜向按主轴归类；0 无
int32_t moveQuadrantFromInput(const InputComponent* input)
{
    if (!input)
        return 0;
    const bool left  = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT));
    const bool right = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT));
    const bool up    = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP));
    const bool down  = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN));

    float vx = 0.0f, vy = 0.0f;
    if (left && right)
        vx = (input->queryKeyPressedDurationMs(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)) <=
              input->queryKeyPressedDurationMs(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
                 ? -1.0f
                 : 1.0f;
    else if (left)
        vx = -1.0f;
    else if (right)
        vx = 1.0f;

    if (up && down)
        vy = (input->queryKeyPressedDurationMs(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)) <=
              input->queryKeyPressedDurationMs(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN)))
                 ? 1.0f
                 : -1.0f;
    else if (up)
        vy = 1.0f;
    else if (down)
        vy = -1.0f;

    if (vx == 0.0f && vy == 0.0f)
        return 0;
    if (std::fabs(vx) >= std::fabs(vy))
        return vx > 0.0f ? 1 : 3;
    return vy > 0.0f ? 2 : 4;
}

bool anyMoveKeyDown(const InputComponent* input)
{
    if (!input)
        return false;
    return input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)) ||
           input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)) ||
           input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)) ||
           input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN));
}

bool anyMoveJustPressed(const InputComponent* input)
{
    return justPressed(input, static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)) ||
           justPressed(input, static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)) ||
           justPressed(input, static_cast<int32_t>(INPUT_SLOT_MOVE_UP)) ||
           justPressed(input, static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN));
}

bool isSameSide(int32_t a, int32_t b)
{
    return a != 0 && a == b;
}

void playBranchAnim(BehaviorComponent* behavior, AvatarComponent* avatar)
{
    if (!behavior || !behavior->behaviorTemplate || !avatar)
        return;
    const auto& branches = behavior->behaviorTemplate->branches;
    for (size_t i = 0; i < branches.size(); ++i)
    {
        const auto& b = branches[i];
        if (b.kind != behavior->currentKind)
            continue;
        if (b.requireTags && (behavior->statusTags & b.requireTags) != b.requireTags)
            continue;
        if (b.denyTags && (behavior->statusTags & b.denyTags) != 0)
            continue;
        if (behavior->currentBranchIndex != static_cast<int32_t>(i) && !b.animation.empty())
            avatar->play(b.animation, b.loop ? -1 : 1, false);
        behavior->currentBranchIndex = static_cast<int32_t>(i);
        return;
    }
}

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

bool skillHasOrderControl(const SkillAttackConfig* cfg, int32_t controlType)
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

void syncInterruptFlags(Entity* entity, BehaviorComponent* behavior)
{
    if (!behavior)
        return;
    auto it = s_runners.find(entity);
    if (it == s_runners.end())
    {
        behavior->interruptOpen      = false;
        behavior->interruptExtraOpen = false;
        return;
    }
    behavior->interruptOpen      = it->second.isInterruptOpen();
    behavior->interruptExtraOpen = it->second.isInterruptExtraOpen();
}
}  // namespace

BehaviorSystem::BehaviorSystem() {}
BehaviorSystem::~BehaviorSystem() {}

void BehaviorSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, BehaviorComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, AvatarComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, TransformComponent);
}

void BehaviorSystem::onEntityAdded(Entity* entity)
{
    s_runners.emplace(entity, ActionRunner{});
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (behavior && behavior->statusTags == 0)
        behavior->statusTags =
            StateTag::kTagGrounded | StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
}

void BehaviorSystem::onEntityRemoved(Entity* entity)
{
    s_runners.erase(entity);
}

void BehaviorSystem::update()
{
    const int32_t dtMs           = getECSManager()->getLastUpdateTimeMs();
    const int64_t runningTimeMs  = getECSManager()->getRunningTimeMs();

    for (Entity* entity : entities)
    {
        processPendingHits(entity);
        tickHitRecovery(entity, dtMs);
        tickSkillCooldowns(entity, dtMs);
        updateAirborneTags(entity);
        updateDoubleTapRun(entity, runningTimeMs);

        auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
        if (behavior && behavior->activeSkillAttackId > 0)
            syncInterruptFlags(entity, behavior);

        tryAbortAttackForLocomotion(entity);
        tryCastFromInput(entity);
        selectBranch(entity);

        behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
        if (!behavior)
            continue;
        if (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kAttack))
        {
            tickAttack(entity, dtMs);
            behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
        }
        // interruptExtra 切跑/跳后同一帧继续 locomotion
        if (behavior && behavior->currentKind == static_cast<int32_t>(BehaviorKind::kAttack))
            ;
        else if (behavior && (behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState)))
            ;  // 受击/倒地不走 locomotion
        else
            tickLocomotion(entity, dtMs);
    }
}

void BehaviorSystem::updateAirborneTags(Entity* entity)
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
                behavior->landLockMs = kLandLockMs;
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

void BehaviorSystem::updateDoubleTapRun(Entity* entity, int64_t runningTimeMs)
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
        if (anyMoveKeyDown(input))
            behavior->statusTags |= StateTag::kTagDashState;
        else
            behavior->statusTags &= ~StateTag::kTagDashState;
        return;
    }

    if (anyMoveJustPressed(input))
    {
        const int32_t q = moveQuadrantFromInput(input);
        if (q != 0)
        {
            if (behavior->lastMovePressMs > 0 &&
                (runningTimeMs - behavior->lastMovePressMs) <= kMoveDoubleTapMs &&
                isSameSide(q, behavior->lastMoveQuadrant))
            {
                behavior->statusTags |= StateTag::kTagDashState;
            }
            behavior->lastMoveQuadrant = q;
            behavior->lastMovePressMs  = runningTimeMs;
        }
    }

    if (!anyMoveKeyDown(input))
        behavior->statusTags &= ~StateTag::kTagDashState;
}

void BehaviorSystem::processPendingHits(Entity* entity)
{
    auto* skillState = MG_GET_COMPONENT(entity, SkillStateComponent);
    auto* behavior   = MG_GET_COMPONENT(entity, BehaviorComponent);
    if (!skillState || !behavior || skillState->pendingHits.empty())
        return;

    auto bestIt = skillState->pendingHits.begin();
    for (auto it = bestIt + 1; it != skillState->pendingHits.end(); ++it)
    {
        if (static_cast<int8_t>(it->hitType) > static_cast<int8_t>(bestIt->hitType))
            bestIt = it;
    }
    const PendingHitInfo hit = *bestIt;
    skillState->pendingHits.clear();

    if ((behavior->statusTags & StateTag::kTagHitState) && hit.hitType <= skillState->activeHitType)
        return;

    skillState->activeHitType   = hit.hitType;
    skillState->activeHitstunMs = hit.hitstunMs;

    behavior->statusTags |= StateTag::kTagHitState;
    behavior->statusTags &=
        ~(StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed | StateTag::kTagDashState);
    behavior->activeSkillAttackId  = 0;
    behavior->pendingSkillAttackId = 0;
    behavior->hitStunRemainingMs   = std::max(0, hit.hitstunMs);
    behavior->downRemainMs         = 0;
    behavior->getUpRemainMs        = 0;

    auto runnerIt = s_runners.find(entity);
    if (runnerIt != s_runners.end())
    {
        auto* avatar = MG_GET_COMPONENT(entity, AvatarComponent);
        auto* disp   = MG_GET_COMPONENT(entity, DisplacementComponent);
        auto* buff   = MG_GET_COMPONENT(entity, BuffComponent);
        runnerIt->second.stop(entity, avatar, disp, buff);
    }

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

    const bool isLaunch = hit.hitType == HitType::kHitLaunch || hit.impulseZ > 0.0f;
    const bool isDown   = hit.hitType == HitType::kHitDown;

    if (physics)
    {
        physics->impulseVelocity.x = hit.impulseX;
        physics->impulseVelocity.z = hit.impulseZ;
        if (isLaunch && hit.impulseZ > 0.0f)
            physics->onGround = 0;
    }

    if (isLaunch)
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kHitUp);
        behavior->statusTags |= StateTag::kTagAirborne;
        behavior->statusTags &= ~StateTag::kTagGrounded;
        if (avatar)
            avatar->play("hit", 1, false);
    }
    else if (isDown)
    {
        behavior->currentKind   = static_cast<int32_t>(BehaviorKind::kHitFloor);
        behavior->statusTags |= StateTag::kTagDownState;
        behavior->downRemainMs  = kDownMs;
        behavior->hitStunRemainingMs = 0;
        if (avatar)
            avatar->play("hit", 1, false);
    }
    else
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kStun);
        if (avatar)
            avatar->play("hit", 1, false);
    }
}

void BehaviorSystem::tickHitRecovery(Entity* entity, int32_t dtMs)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* physics  = MG_GET_COMPONENT(entity, PhysicsComponent);
    auto* avatar   = MG_GET_COMPONENT(entity, AvatarComponent);
    if (!behavior)
        return;

    if (behavior->landLockMs > 0)
        behavior->landLockMs = std::max(0, behavior->landLockMs - dtMs);

    // 击飞：落地后进倒地
    if ((behavior->statusTags & StateTag::kTagHitState) &&
        behavior->currentKind == static_cast<int32_t>(BehaviorKind::kHitUp) && physics && physics->onGround)
    {
        behavior->currentKind  = static_cast<int32_t>(BehaviorKind::kHitFloor);
        behavior->statusTags |= StateTag::kTagDownState;
        behavior->statusTags &= ~(StateTag::kTagAirborne | StateTag::kTagFalling);
        behavior->statusTags |= StateTag::kTagGrounded;
        behavior->downRemainMs = kDownMs;
        behavior->hitStunRemainingMs = 0;
        playBranchAnim(behavior, avatar);
        return;
    }

    // 普通硬直倒计时
    if (behavior->hitStunRemainingMs > 0)
    {
        behavior->hitStunRemainingMs = std::max(0, behavior->hitStunRemainingMs - dtMs);
        if (behavior->hitStunRemainingMs == 0 &&
            behavior->currentKind == static_cast<int32_t>(BehaviorKind::kStun))
        {
            behavior->statusTags &= ~StateTag::kTagHitState;
            behavior->statusTags |=
                StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
            behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
            if (auto* ss = MG_GET_COMPONENT(entity, SkillStateComponent))
                ss->activeHitType = HitType::kHitNone;
        }
        return;
    }

    // 倒地 → 起身
    if (behavior->downRemainMs > 0)
    {
        behavior->downRemainMs = std::max(0, behavior->downRemainMs - dtMs);
        if (behavior->downRemainMs == 0)
        {
            behavior->currentKind  = static_cast<int32_t>(BehaviorKind::kGetUp);
            behavior->getUpRemainMs = kGetUpMs;
            playBranchAnim(behavior, avatar);
        }
        return;
    }

    if (behavior->getUpRemainMs > 0)
    {
        behavior->getUpRemainMs = std::max(0, behavior->getUpRemainMs - dtMs);
        if (behavior->getUpRemainMs == 0)
        {
            behavior->statusTags &= ~(StateTag::kTagHitState | StateTag::kTagDownState);
            behavior->statusTags |=
                StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed | StateTag::kTagGrounded;
            behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
            if (auto* ss = MG_GET_COMPONENT(entity, SkillStateComponent))
                ss->activeHitType = HitType::kHitNone;
        }
    }
}

void BehaviorSystem::tickSkillCooldowns(Entity* entity, int32_t dtMs)
{
    auto* deck = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!deck)
        return;
    for (auto& e : deck->skills)
    {
        if (e.coolDownMs > 0)
            e.coolDownMs = std::max(0, e.coolDownMs - dtMs);
    }
}

int32_t BehaviorSystem::resolveSkillFromSlot(Entity* entity, int32_t inputSlot, int32_t* outStep) const
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
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
    if (behavior && behavior->activeInputSlot == inputSlot && behavior->activeSkillAttackId > 0)
        step = behavior->activeStepInSlot + 1;
    if (step < 0 || step >= static_cast<int32_t>(indices.size()))
        step = 0;

    const int32_t deckIndex = indices[static_cast<size_t>(step)];
    if (deckIndex < 0 || deckIndex >= static_cast<int32_t>(deck->skills.size()))
        return 0;

    if (outStep)
        *outStep = step;
    return deck->skills[static_cast<size_t>(deckIndex)].skillAttackId;
}

bool BehaviorSystem::beginSkill(Entity* entity, int32_t skillId, int32_t inputSlot, int32_t stepInSlot)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* avatar   = MG_GET_COMPONENT(entity, AvatarComponent);
    auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
    auto* attr     = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!behavior || skillId <= 0)
        return false;

    const auto* skillAtk = Config::getInstance()->getSkillAttackConfigById(skillId);
    if (!skillAtk)
        return false;

    SkillDeckEntry* entry = findDeckEntry(deck, skillId);
    if (entry)
    {
        if (entry->coolDownMs > 0)
            return false;
        if (entry->releaseCount <= 0)
            return false;
    }

    if (attr)
    {
        if (skillAtk->mp > 0 && attr->currentAttribute.mp < static_cast<float>(skillAtk->mp))
            return false;
        if (skillAtk->ep > 0 && attr->ep < static_cast<float>(skillAtk->ep))
            return false;
    }

    auto it = s_runners.find(entity);
    if (it == s_runners.end())
        return false;

    auto* disp = MG_GET_COMPONENT(entity, DisplacementComponent);
    auto* buff = MG_GET_COMPONENT(entity, BuffComponent);
    if (!it->second.start(skillId, entity, avatar, disp, buff))
        return false;

    if (attr)
    {
        if (skillAtk->mp > 0)
            attr->currentAttribute.mp -= static_cast<float>(skillAtk->mp);
        if (skillAtk->ep > 0)
            attr->ep = std::max(0.0f, attr->ep - static_cast<float>(skillAtk->ep));
    }
    if (entry)
    {
        if (entry->releaseMax > 0)
            entry->releaseCount = std::max(0, entry->releaseCount - 1);
        if (entry->coolDownMaxMs > 0 && entry->releaseCount <= 0)
        {
            entry->coolDownMs   = entry->coolDownMaxMs;
            entry->releaseCount = entry->releaseMax;
        }
    }

    behavior->activeSkillAttackId  = skillId;
    behavior->pendingSkillAttackId = 0;
    behavior->activeInputSlot      = inputSlot;
    behavior->activeStepInSlot     = stepInSlot;
    behavior->currentKind          = static_cast<int32_t>(BehaviorKind::kAttack);
    behavior->statusTags &= ~StateTag::kTagMovable;
    // 攻击中保留 AttackAllowed 以便预输入
    return true;
}

void BehaviorSystem::tryCastFromInput(Entity* entity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* input    = MG_GET_COMPONENT(entity, InputComponent);
    auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
    auto* skillBar = MG_GET_COMPONENT(entity, SkillBarComponent);
    if (!behavior || !input || !deck || deck->skills.empty())
        return;
    if ((behavior->statusTags & StateTag::kTagAttackAllowed) == 0)
        return;
    if (behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState))
        return;
    if (behavior->landLockMs > 0)
        return;

    int32_t skillId   = 0;
    int32_t inputSlot = 0;
    int32_t step      = 0;

    if (skillBar)
    {
        for (const auto& slot : skillBar->skillSlots)
        {
            if (slot.slotIndex <= 0 || !justPressed(input, slot.slotIndex))
                continue;
            int32_t s = 0;
            const int32_t id = resolveSkillFromSlot(entity, slot.slotIndex, &s);
            if (id <= 0)
                continue;
            skillId   = id;
            inputSlot = slot.slotIndex;
            step      = s;
            break;
        }
    }

    if (skillId <= 0 && justPressed(input, static_cast<int32_t>(INPUT_SLOT_0)))
    {
        // 跑中突刺：DashState + A → thrustSkillAttackId（若有）
        if ((behavior->statusTags & StateTag::kTagDashState) && behavior->thrustSkillAttackId > 0)
            skillId = behavior->thrustSkillAttackId;
        else
            skillId = deck->skills.front().skillAttackId;
        inputSlot = static_cast<int32_t>(INPUT_SLOT_0);
        step      = 0;
    }

    // skillBar 解析到槽 0 时同样做跑攻重映射
    if (skillId > 0 && inputSlot == static_cast<int32_t>(INPUT_SLOT_0) &&
        (behavior->statusTags & StateTag::kTagDashState) && behavior->thrustSkillAttackId > 0)
    {
        skillId = behavior->thrustSkillAttackId;
        step    = 0;
    }

    if (skillId <= 0)
        return;

    // 攻击中：写入预输入，等取消窗消费
    if (behavior->activeSkillAttackId > 0)
    {
        behavior->pendingSkillAttackId = skillId;
        behavior->activeInputSlot      = inputSlot;
        behavior->activeStepInSlot     = step;
        return;
    }

    behavior->pendingSkillAttackId = skillId;
    behavior->activeInputSlot      = inputSlot;
    behavior->activeStepInSlot     = step;
}

void BehaviorSystem::selectBranch(Entity* entity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* avatar   = MG_GET_COMPONENT(entity, AvatarComponent);
    auto* input    = MG_GET_COMPONENT(entity, InputComponent);
    if (!behavior || !behavior->behaviorTemplate)
        return;

    if (behavior->activeSkillAttackId > 0)
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kAttack);
        return;
    }

    if (behavior->pendingSkillAttackId > 0 &&
        !(behavior->statusTags & (StateTag::kTagHitState | StateTag::kTagDownState)))
    {
        const int32_t skillId = behavior->pendingSkillAttackId;
        const int32_t slot    = behavior->activeInputSlot;
        const int32_t step    = behavior->activeStepInSlot;
        behavior->pendingSkillAttackId = 0;
        if (!beginSkill(entity, skillId, slot, step))
        {
            behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed;
        }
        return;
    }

    if (behavior->statusTags & StateTag::kTagDownState)
    {
        if (behavior->getUpRemainMs > 0)
            behavior->currentKind = static_cast<int32_t>(BehaviorKind::kGetUp);
        else
            behavior->currentKind = static_cast<int32_t>(BehaviorKind::kHitFloor);
        playBranchAnim(behavior, avatar);
        return;
    }

    if (behavior->statusTags & StateTag::kTagHitState)
    {
        if (behavior->currentKind != static_cast<int32_t>(BehaviorKind::kHitUp) &&
            behavior->currentKind != static_cast<int32_t>(BehaviorKind::kHitFloor) &&
            behavior->currentKind != static_cast<int32_t>(BehaviorKind::kGetUp))
            behavior->currentKind = static_cast<int32_t>(BehaviorKind::kStun);
        playBranchAnim(behavior, avatar);
        return;
    }

    // 跳跃（Z）
    auto* physics = MG_GET_COMPONENT(entity, PhysicsComponent);
    auto* attr    = MG_GET_COMPONENT(entity, AttributeComponent);
    if (physics && physics->onGround && justPressed(input, static_cast<int32_t>(INPUT_SLOT_Z)) &&
        (behavior->statusTags & StateTag::kTagMovable))
    {
        float jump = attr ? attr->currentAttribute.jumpSpeed : 0.0f;
        // 表里 jumpSpeed 量级偏小（相对 gravity=1800），过低则用默认
        if (jump < 400.0f)
            jump = kDefaultJumpSpeed;
        physics->velocity.z = jump;
        physics->onGround   = 0;
        behavior->statusTags |= StateTag::kTagAirborne;
        behavior->statusTags &= ~StateTag::kTagGrounded;
    }

    if (behavior->statusTags & StateTag::kTagAirborne)
    {
        // 模板可能无跳跃分支，仍标 Walk/Idle 动画；逻辑以 Airborne 为准
        if (anyMoveKeyDown(input))
            behavior->currentKind = (behavior->statusTags & StateTag::kTagDashState)
                                        ? static_cast<int32_t>(BehaviorKind::kDash)
                                        : static_cast<int32_t>(BehaviorKind::kWalk);
        else
            behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
    }
    else if (anyMoveKeyDown(input))
    {
        behavior->currentKind = (behavior->statusTags & StateTag::kTagDashState)
                                    ? static_cast<int32_t>(BehaviorKind::kDash)
                                    : static_cast<int32_t>(BehaviorKind::kWalk);
    }
    else
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
    }

    playBranchAnim(behavior, avatar);
}

void BehaviorSystem::tickLocomotion(Entity* entity, int32_t /*dtMs*/)
{
    auto* behavior  = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* physics   = MG_GET_COMPONENT(entity, PhysicsComponent);
    auto* input     = MG_GET_COMPONENT(entity, InputComponent);
    auto* attribute = MG_GET_COMPONENT(entity, AttributeComponent);
    auto* transform = MG_GET_COMPONENT(entity, TransformComponent);
    if (!behavior || !physics || !input)
        return;

    if ((behavior->statusTags & StateTag::kTagMovable) == 0 || behavior->landLockMs > 0)
    {
        physics->velocity.x = 0;
        physics->velocity.y = 0;
        return;
    }

    float speed = 300.0f;
    if (attribute)
        speed = attribute->currentAttribute.moveSpeed;
    else if (behavior->roleConfig)
        speed = behavior->roleConfig->attribute.moveSpeed;

    if (behavior->statusTags & StateTag::kTagDashState)
        speed *= kRunRate;
    if (behavior->statusTags & StateTag::kTagAirborne)
        speed *= kAirControl;

    const int32_t leftSlot  = static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT);
    const int32_t rightSlot = static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT);
    const int32_t upSlot    = static_cast<int32_t>(INPUT_SLOT_MOVE_UP);
    const int32_t downSlot  = static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN);

    const bool left  = input->isKeyDown(leftSlot);
    const bool right = input->isKeyDown(rightSlot);
    const bool up    = input->isKeyDown(upSlot);
    const bool down  = input->isKeyDown(downSlot);

    float vx = 0.0f;
    if (left && right)
        vx = (input->queryKeyPressedDurationMs(leftSlot) <= input->queryKeyPressedDurationMs(rightSlot)) ? -1.0f : 1.0f;
    else if (left)
        vx = -1.0f;
    else if (right)
        vx = 1.0f;

    float vy = 0.0f;
    if (up && down)
        vy = (input->queryKeyPressedDurationMs(upSlot) <= input->queryKeyPressedDurationMs(downSlot)) ? 1.0f : -1.0f;
    else if (up)
        vy = 1.0f;
    else if (down)
        vy = -1.0f;

    if (vx != 0 || vy != 0)
    {
        const float len = std::sqrt(vx * vx + vy * vy);
        vx              = vx / len * speed;
        vy              = vy / len * speed;
        if (transform && vx != 0 && (behavior->statusTags & StateTag::kTagFacingAllowed))
            transform->facingDirection = vx > 0 ? FacingDirection::kFacingRight : FacingDirection::kFacingLeft;
    }
    physics->velocity.x = vx;
    physics->velocity.y = vy;
}

void BehaviorSystem::tickAttack(Entity* entity, int32_t dtMs)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* avatar   = MG_GET_COMPONENT(entity, AvatarComponent);
    auto* input    = MG_GET_COMPONENT(entity, InputComponent);
    auto* physics  = MG_GET_COMPONENT(entity, PhysicsComponent);
    auto it        = s_runners.find(entity);
    if (!behavior || it == s_runners.end())
        return;

    auto* disp                   = MG_GET_COMPONENT(entity, DisplacementComponent);
    auto* buff                   = MG_GET_COMPONENT(entity, BuffComponent);
    behavior->interruptOpen      = it->second.isInterruptOpen();
    behavior->interruptExtraOpen = it->second.isInterruptExtraOpen();

    // 技能中控制移动（action.control + controlVelocity）
    if (physics && input)
    {
        const int32_t actionId = it->second.getCurrentActionId();
        if (const auto* actionCfg = Config::getInstance()->getActionAttackConfigById(actionId))
        {
            if (actionCfg->control != 0 && actionCfg->controlVelocity > 0.0f)
            {
                const float speed = actionCfg->controlVelocity;
                float vx = 0.0f, vy = 0.0f;
                if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)))
                    vx -= 1.0f;
                if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)))
                    vx += 1.0f;
                if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)))
                    vy += 1.0f;
                if (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN)))
                    vy -= 1.0f;
                if (vx != 0.0f || vy != 0.0f)
                {
                    const float len = std::sqrt(vx * vx + vy * vy);
                    physics->velocity.x = vx / len * speed;
                    physics->velocity.y = vy / len * speed;
                    if (auto* tf = MG_GET_COMPONENT(entity, TransformComponent))
                    {
                        if (vx != 0.0f)
                            tf->facingDirection =
                                vx > 0 ? FacingDirection::kFacingRight : FacingDirection::kFacingLeft;
                    }
                }
                else
                {
                    physics->velocity.x = 0;
                    physics->velocity.y = 0;
                }
            }
            else
            {
                physics->velocity.x = 0;
                physics->velocity.y = 0;
            }
        }
    }

    // 取消窗：sorder / sorderControlType 允许时切技能
    if (behavior->pendingSkillAttackId > 0 &&
        canInterruptActive(behavior->pendingSkillAttackId, behavior->activeSkillAttackId, behavior->interruptOpen,
                           behavior->interruptExtraOpen))
    {
        const int32_t nextId = behavior->pendingSkillAttackId;
        const int32_t slot   = behavior->activeInputSlot;
        const int32_t step   = behavior->activeStepInSlot;
        behavior->pendingSkillAttackId = 0;
        if (beginSkill(entity, nextId, slot, step))
            return;
    }

    if (it->second.tick(dtMs, entity, avatar, disp, buff))
    {
        const int32_t finishedSkill = behavior->activeSkillAttackId;
        behavior->activeSkillAttackId = 0;
        behavior->currentKind         = static_cast<int32_t>(BehaviorKind::kIdle);
        behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed;
        if (physics && physics->onGround)
            behavior->statusTags |= StateTag::kTagGrounded;

        // next_skill：结束时仍按住同槽 → 自动衔接
        if (finishedSkill > 0 && input && behavior->activeInputSlot > 0 &&
            input->isKeyDown(behavior->activeInputSlot))
        {
            if (auto* entry = findDeckEntry(MG_GET_COMPONENT(entity, SkillDeckComponent), finishedSkill))
            {
                if (entry->nextSkillAttackId > 0)
                {
                    behavior->pendingSkillAttackId = entry->nextSkillAttackId;
                    behavior->activeStepInSlot += 1;
                }
            }
        }
    }
}

bool BehaviorSystem::canInterruptActive(int32_t pendingId,
                                        int32_t activeId,
                                        bool interruptOpen,
                                        bool interruptExtraOpen) const
{
    if (pendingId <= 0 || activeId <= 0)
        return false;

    const auto* pendingCfg = Config::getInstance()->getSkillAttackConfigById(pendingId);
    const auto* activeCfg  = Config::getInstance()->getSkillAttackConfigById(activeId);
    if (!pendingCfg)
        return false;

    // sorderControlType：普通窗 / extra 窗忽略 order
    if (interruptOpen && skillHasOrderControl(pendingCfg, kIgnoreOrderInterruptFrame))
        return true;
    if (interruptExtraOpen && skillHasOrderControl(pendingCfg, kIgnoreOrderInterruptExtraFrame))
        return true;

    if (!interruptOpen)
        return false;

    // 对齐黑月 SkillBase:isPriority：新技能 sorder==-1 或 >= 当前
    const int32_t pendingOrder = pendingCfg->sorder;
    const int32_t activeOrder  = activeCfg ? activeCfg->sorder : 0;
    if (pendingOrder == -1 || activeOrder == -1)
        return true;
    return pendingOrder >= activeOrder;
}

bool BehaviorSystem::tryAbortAttackForLocomotion(Entity* entity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* input    = MG_GET_COMPONENT(entity, InputComponent);
    if (!behavior || !input || behavior->activeSkillAttackId <= 0)
        return false;
    if (!behavior->interruptExtraOpen)
        return false;

    const bool wantJump = justPressed(input, static_cast<int32_t>(INPUT_SLOT_Z));
    const bool wantRun =
        (behavior->statusTags & StateTag::kTagDashState) != 0 && anyMoveKeyDown(input);
    if (!wantJump && !wantRun)
        return false;

    auto it = s_runners.find(entity);
    if (it == s_runners.end())
        return false;

    auto* avatar = MG_GET_COMPONENT(entity, AvatarComponent);
    auto* disp   = MG_GET_COMPONENT(entity, DisplacementComponent);
    auto* buff   = MG_GET_COMPONENT(entity, BuffComponent);
    it->second.stop(entity, avatar, disp, buff);

    behavior->activeSkillAttackId  = 0;
    behavior->pendingSkillAttackId = 0;
    behavior->interruptOpen        = false;
    behavior->interruptExtraOpen   = false;
    behavior->currentKind          = static_cast<int32_t>(BehaviorKind::kIdle);
    behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;
    return true;
}

NS_MG_END
