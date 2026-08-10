#include "AISystem.h"

#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/ai/SkillAiRules.h"
#include "mugen/bt/SkillCastRules.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace
{
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

SkillAiRuntimeState& findOrCreateAiState(AIComponent* aiComp, int32_t skillAiId)
{
    for (auto& s : aiComp->skillAiStates)
    {
        if (s.skillAiId == skillAiId)
            return s;
    }
    SkillAiRuntimeState state;
    state.skillAiId = skillAiId;
    aiComp->skillAiStates.push_back(state);
    return aiComp->skillAiStates.back();
}

void bindAiConfig(AIComponent* aiComp, const AiConfig* ai)
{
    if (!aiComp || !ai)
        return;
    if (aiComp->aiConfigId == ai->id)
        return;
    aiComp->aiConfigId = ai->id;
    aiComp->skillAiStates.clear();
    for (int32_t skillAiId : ai->skillAiIds)
    {
        if (skillAiId <= 0)
            continue;
        if (const auto* cfg = Config::getInstance()->getSkillAiConfigById(skillAiId))
        {
            SkillAiRuntimeState st;
            SkillAiRules::ensureRuntime(st, cfg);
            aiComp->skillAiStates.push_back(st);
        }
    }
    if (ai->patrolScope > 0)
        aiComp->patrolScope = ai->patrolScope;
    else if (aiComp->patrolScope <= 0 && ai->chaseScopeX.y > 0)
        aiComp->patrolScope = (std::max)(150, ai->chaseScopeX.y / 4);
}

Random& worldRandom(ECSManager* ecs)
{
    static Random fallback;
    if (!ecs)
        return fallback;
    if (auto* word = reinterpret_cast<GameWord*>(ecs->getUserdata()))
        return word->random;
    return fallback;
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
    Random& rng        = worldRandom(getECSManager());

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

        auto* aiComp = MG_GET_COMPONENT(entity, AIComponent);
        if (!aiComp)
            aiComp = MG_ADD_COMPONENT(entity, AIComponent);

        if (aiComp->castIntervalRemainMs > 0)
            aiComp->castIntervalRemainMs = (std::max)(0, aiComp->castIntervalRemainMs - dtMs);

        const AiConfig* ai = nullptr;
        if (behavior->roleConfig && !behavior->roleConfig->aiIds.empty())
            ai = Config::getInstance()->getAiConfigById(behavior->roleConfig->aiIds.front());
        if (ai)
            bindAiConfig(aiComp, ai);

        for (auto& st : aiComp->skillAiStates)
            SkillAiRules::tick(st, dtMs);

        if (aiComp->castIntervalRemainMs > 0)
            continue;

        auto* selfTf = MG_GET_COMPONENT(entity, TransformComponent);
        Entity* player = selfTf ? findNearestPlayer(getECSManager(), selfTf) : nullptr;
        if (ai && selfTf && player)
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

        int32_t skillId = 0;

        // skillIds[i] 与 skillAiIds[i] 按下标配对
        if (ai && !ai->skillIds.empty())
        {
            const size_t n = ai->skillIds.size();
            for (size_t i = 0; i < n; ++i)
            {
                const int32_t sid = ai->skillIds[i];
                if (sid <= 0)
                    continue;

                const int32_t skillAiId =
                    (i < ai->skillAiIds.size()) ? ai->skillAiIds[i] : 0;
                if (skillAiId > 0)
                {
                    const auto* skillAiCfg =
                        Config::getInstance()->getSkillAiConfigById(skillAiId);
                    if (!skillAiCfg)
                        continue;
                    auto& st = findOrCreateAiState(aiComp, skillAiId);
                    if (!SkillAiRules::check(entity, player, st, skillAiCfg, rng))
                        continue;
                }

                if (!SkillCastRules::isAllowCast(entity, sid, true))
                    continue;

                skillId = sid;
                break;
            }
        }

        if (skillId <= 0 && !deck->skills.empty())
            skillId = deck->skills.front().skillAttackId;

        if (skillId > 0 && SkillCastRules::isAllowCast(entity, skillId, true))
        {
            SkillCastRules::presetSkill(entity, skillId, static_cast<int32_t>(INPUT_SLOT_0), 0);
            aiComp->castIntervalRemainMs =
                ai && ai->skillInterval > 0 ? ai->skillInterval : 1200;
        }
    }
}

NS_MG_END
