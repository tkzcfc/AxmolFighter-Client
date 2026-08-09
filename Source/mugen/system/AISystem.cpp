#include "AISystem.h"
#include "mugen/Components.h"
#include "mugen/bt/SkillCastRules.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"

#include <cmath>
#include <unordered_map>

NS_MG_BEGIN

namespace
{
std::unordered_map<EntityId, int32_t> s_aiCooldownMs;

Entity* findNearestPlayer(ECSManager* ecs, const TransformComponent* selfTf)
{
    if (!ecs || !selfTf)
        return nullptr;
    Entity* best     = nullptr;
    int64_t bestDist = 0;
    Signature sig;
    sig.set(ecs->getComponentTypeId("IdentityComponent"));
    sig.set(ecs->getComponentTypeId("TransformComponent"));
    for (Entity* e : ecs->getEntitiesBySignature(sig))
    {
        auto* id = MG_GET_COMPONENT(e, IdentityComponent);
        auto* tf = MG_GET_COMPONENT(e, TransformComponent);
        if (!id || !tf || id->category != EntityCategory::kPlayer)
            continue;
        const int64_t dx   = static_cast<int64_t>(tf->position.x) - selfTf->position.x;
        const int64_t dy   = static_cast<int64_t>(tf->position.y) - selfTf->position.y;
        const int64_t dist = dx * dx + dy * dy;
        if (!best || dist < bestDist)
        {
            best     = e;
            bestDist = dist;
        }
    }
    return best;
}
}  // namespace

AISystem::AISystem() {}
AISystem::~AISystem() {}

void AISystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, BehaviorComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, SkillCastComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, SkillDeckComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, IdentityComponent);
}

void AISystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();

    for (Entity* entity : entities)
    {
        auto* identity = MG_GET_COMPONENT(entity, IdentityComponent);
        auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
        auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
        auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
        if (!identity || !behavior || !cast || !deck)
            continue;
        if (identity->category != EntityCategory::kMonster)
            continue;
        if (cast->activeSkillAttackId > 0 || cast->pendingSkillAttackId > 0)
            continue;
        if (deck->skills.empty())
            continue;
        if ((behavior->statusTags & StateTag::kTagAttackAllowed) == 0)
            continue;
        if (behavior->statusTags & StateTag::kTagHitState)
            continue;

        int32_t& cd = s_aiCooldownMs[entity->getId()];
        if (cd > 0)
        {
            cd = (std::max)(0, cd - dtMs);
            continue;
        }

        const AiConfig* ai = nullptr;
        if (behavior->roleConfig && !behavior->roleConfig->aiIds.empty())
            ai = Config::getInstance()->getAiConfigById(behavior->roleConfig->aiIds.front());

        auto* selfTf = MG_GET_COMPONENT(entity, TransformComponent);
        if (ai && selfTf)
        {
            if (Entity* player = findNearestPlayer(getECSManager(), selfTf))
            {
                auto* playerTf = MG_GET_COMPONENT(player, TransformComponent);
                if (playerTf)
                {
                    const int dx   = std::abs(selfTf->position.x - playerTf->position.x);
                    const int dz   = std::abs(selfTf->position.y - playerTf->position.y);
                    const int maxX = ai->chaseScopeX.y > 0 ? ai->chaseScopeX.y : 2000;
                    const int maxZ = ai->chaseScopeZ.y > 0 ? ai->chaseScopeZ.y : 2000;
                    if (dx > maxX || dz > maxZ)
                        continue;
                }
            }
        }

        int32_t skillId = deck->skills.front().skillAttackId;
        if (ai)
        {
            for (int32_t sid : ai->skillIds)
            {
                if (sid > 0)
                {
                    skillId = sid;
                    break;
                }
            }
        }

        if (skillId > 0)
        {
            SkillCastRules::presetSkill(entity, skillId, static_cast<int32_t>(INPUT_SLOT_0), 0);
            cd = ai && ai->skillInterval > 0 ? ai->skillInterval : 1200;
        }
    }
}

NS_MG_END
