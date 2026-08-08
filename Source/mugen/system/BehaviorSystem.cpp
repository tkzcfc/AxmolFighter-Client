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
}

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
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    for (Entity* entity : entities)
    {
        processPendingHits(entity);
        if (auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent); behavior && behavior->hitStunRemainingMs > 0)
        {
            behavior->hitStunRemainingMs = std::max(0, behavior->hitStunRemainingMs - dtMs);
            if (behavior->hitStunRemainingMs == 0)
            {
                behavior->statusTags &= ~StateTag::kTagHitState;
                behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed;
            }
        }
        tryCastFromInput(entity);
        selectBranch(entity);
        auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
        if (!behavior)
            continue;
        if (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kAttack))
            tickAttack(entity, dtMs);
        else
            tickLocomotion(entity, dtMs);
    }
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
    behavior->statusTags &= ~(StateTag::kTagMovable | StateTag::kTagAttackAllowed);
    behavior->activeSkillAttackId  = 0;
    behavior->pendingSkillAttackId = 0;
    behavior->currentKind          = static_cast<int32_t>(BehaviorKind::kStun);
    behavior->hitStunRemainingMs   = std::max(0, hit.hitstunMs);

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

    if (auto* physics = MG_GET_COMPONENT(entity, PhysicsComponent))
    {
        physics->impulseVelocity.x = hit.impulseX;
        physics->impulseVelocity.z = hit.impulseZ;
        if (hit.impulseZ > 0.0f)
            physics->onGround = 0;
    }

    if (auto* avatar = MG_GET_COMPONENT(entity, AvatarComponent))
        avatar->play("hit", 1, false);
}

void BehaviorSystem::tryCastFromInput(Entity* entity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* input    = MG_GET_COMPONENT(entity, InputComponent);
    auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!behavior || !input || !deck || deck->skills.empty())
        return;

    if ((behavior->statusTags & StateTag::kTagAttackAllowed) == 0)
        return;
    if (behavior->statusTags & StateTag::kTagHitState)
        return;

    // 槽位 0 按下 → 释放第一个技能
    if (!input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_0)))
        return;

    const int32_t skillId = deck->skills.front().skillAttackId;
    if (skillId <= 0)
        return;

    if (behavior->activeSkillAttackId > 0)
    {
        behavior->pendingSkillAttackId = skillId;
        return;
    }

    behavior->pendingSkillAttackId = skillId;
}

void BehaviorSystem::selectBranch(Entity* entity)
{
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* avatar   = MG_GET_COMPONENT(entity, AvatarComponent);
    auto* input    = MG_GET_COMPONENT(entity, InputComponent);
    if (!behavior || !behavior->behaviorTemplate)
        return;

    // 攻击中保持 Attack 分支直到 ActionRunner 结束
    if (behavior->activeSkillAttackId > 0)
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kAttack);
        return;
    }

    // 有预输入技能则进入攻击
    if (behavior->pendingSkillAttackId > 0)
    {
        behavior->activeSkillAttackId  = behavior->pendingSkillAttackId;
        behavior->pendingSkillAttackId = 0;
        behavior->currentKind          = static_cast<int32_t>(BehaviorKind::kAttack);
        behavior->statusTags |= StateTag::kTagGrounded;
        behavior->statusTags &= ~StateTag::kTagMovable;
        auto it = s_runners.find(entity);
        if (it != s_runners.end())
        {
            auto* disp = MG_GET_COMPONENT(entity, DisplacementComponent);
            auto* buff = MG_GET_COMPONENT(entity, BuffComponent);
            auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
            if (const auto* skillAtk = Config::getInstance()->getSkillAttackConfigById(behavior->activeSkillAttackId))
            {
                if (attr && skillAtk->mp > 0 && attr->currentAttribute.mp < skillAtk->mp)
                {
                    behavior->activeSkillAttackId = 0;
                    behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed;
                    return;
                }
                if (attr && skillAtk->mp > 0)
                    attr->currentAttribute.mp -= skillAtk->mp;
            }
            if (!it->second.start(behavior->activeSkillAttackId, entity, avatar, disp, buff))
            {
                behavior->activeSkillAttackId = 0;
                behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed;
            }
        }
        return;
    }

    // 受击优先
    if (behavior->statusTags & StateTag::kTagHitState)
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kStun);
    }
    else if (input && (input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT)) ||
                       input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT)) ||
                       input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_UP)) ||
                       input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN))))
    {
        const bool dash = input->isKeyDown(static_cast<int32_t>(INPUT_SLOT_X));
        behavior->currentKind =
            dash ? static_cast<int32_t>(BehaviorKind::kDash) : static_cast<int32_t>(BehaviorKind::kWalk);
    }
    else
    {
        behavior->currentKind = static_cast<int32_t>(BehaviorKind::kIdle);
    }

    // 按模板匹配动画
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
        if (behavior->currentBranchIndex != static_cast<int32_t>(i) && avatar && !b.animation.empty())
            avatar->play(b.animation, b.loop ? -1 : 1, false);
        behavior->currentBranchIndex = static_cast<int32_t>(i);
        break;
    }
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

    if ((behavior->statusTags & StateTag::kTagMovable) == 0)
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

    const int32_t leftSlot  = static_cast<int32_t>(INPUT_SLOT_MOVE_LEFT);
    const int32_t rightSlot = static_cast<int32_t>(INPUT_SLOT_MOVE_RIGHT);
    const int32_t upSlot    = static_cast<int32_t>(INPUT_SLOT_MOVE_UP);
    const int32_t downSlot  = static_cast<int32_t>(INPUT_SLOT_MOVE_DOWN);

    const bool left  = input->isKeyDown(leftSlot);
    const bool right = input->isKeyDown(rightSlot);
    const bool up    = input->isKeyDown(upSlot);
    const bool down  = input->isKeyDown(downSlot);

    // 同时按下对向键：按下时长更短的一侧优先（后按优先，对齐旧 GroundMove）
    float vx = 0.0f;
    if (left && right)
    {
        vx = (input->queryKeyPressedDurationMs(leftSlot) <= input->queryKeyPressedDurationMs(rightSlot)) ? -1.0f : 1.0f;
    }
    else if (left)
        vx = -1.0f;
    else if (right)
        vx = 1.0f;

    float vy = 0.0f;
    if (up && down)
    {
        vy = (input->queryKeyPressedDurationMs(upSlot) <= input->queryKeyPressedDurationMs(downSlot)) ? 1.0f : -1.0f;
    }
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
    auto it        = s_runners.find(entity);
    if (!behavior || it == s_runners.end())
        return;

    auto* disp                   = MG_GET_COMPONENT(entity, DisplacementComponent);
    auto* buff                   = MG_GET_COMPONENT(entity, BuffComponent);
    behavior->interruptOpen      = it->second.isInterruptOpen();
    behavior->interruptExtraOpen = it->second.isInterruptExtraOpen();

    // 取消窗：有预输入则切技能
    if (behavior->pendingSkillAttackId > 0 && behavior->interruptOpen)
    {
        const int32_t nextId           = behavior->pendingSkillAttackId;
        behavior->pendingSkillAttackId = 0;
        behavior->activeSkillAttackId  = nextId;
        it->second.start(nextId, entity, avatar, disp, buff);
        return;
    }

    if (it->second.tick(dtMs, entity, avatar, disp, buff))
    {
        behavior->activeSkillAttackId = 0;
        behavior->currentKind         = static_cast<int32_t>(BehaviorKind::kIdle);
        behavior->statusTags |= StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagGrounded;
        behavior->statusTags &= ~StateTag::kTagHitState;
    }
}

NS_MG_END
