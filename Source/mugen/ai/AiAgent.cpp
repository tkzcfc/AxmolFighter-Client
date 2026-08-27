#include "mugen/ai/AiAgent.h"

#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/component/AIComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/StdC.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"
#include "mugen/skill/SkillManager.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace
{

const AiConfig* loadAiConfig(int32_t id)
{
    if (id <= 0)
        return nullptr;
    return Config::getInstance()->getAiConfigById(id);
}

}  // namespace

void AiAgent::bindConfig(Entity* entity)
{
    const AiConfig* ai = resolveConfig(entity);
    if (!ai)
        return;
    if (aiConfigId == ai->id)
        return;

    aiConfigId = ai->id;
    skillAis.clear();
    for (int32_t skillAiId : ai->skillAiIds)
    {
        if (skillAiId <= 0)
            continue;
        if (const auto* cfg = Config::getInstance()->getSkillAiConfigById(skillAiId))
        {
            auto node = std::make_unique<SkillAi>();
            node->ensure(cfg);
            skillAis.push_back(std::move(node));
        }
    }

    const int32_t px = (std::max)(std::abs(ai->patrolScopeX.x), std::abs(ai->patrolScopeX.y));
    if (px > 0)
        patrolScope = px;
    else if (patrolScope <= 0 && ai->chaseScopeX.y > 0)
        patrolScope = (std::max)(150, ai->chaseScopeX.y / 4);

    skillSlotIntervalMs.assign(ai->skillIds.size(), 0);
}

void AiAgent::update(Entity* entity, int32_t dtMs)
{
    if (!entity)
        return;

    auto* identity = MG_GET_COMPONENT(entity, IdentityComponent);
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* mgr      = SkillManager::of(entity);
    auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!identity || !behavior || !mgr || !deck)
        return;
    if (identity->category != EntityCategory::kMonster)
        return;
    if (mgr->activeSkillAttackId > 0 || mgr->pendingSkillAttackId > 0)
        return;
    if (deck->skills.empty())
        return;
    if ((behavior->statusTags & StateTag::kTagAttackAllowed) == 0)
        return;
    if (behavior->statusTags & StateTag::kTagHitState)
        return;

    for (int32_t& remain : skillSlotIntervalMs)
        remain = (std::max)(0, remain - dtMs);

    for (auto& st : skillAis)
    {
        if (st)
            st->tick(dtMs);
    }

    const AiConfig* ai = config();
    auto* selfTf       = MG_GET_COMPONENT(entity, TransformComponent);
    Entity* player     = selfTf ? findNearestPlayer(entity->getECSManager(), selfTf) : nullptr;
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
                return;
        }
    }

    Random& rng      = worldRandom(entity->getECSManager());
    int32_t skillId  = 0;
    int32_t usedPrio = 0;

    if (ai && !ai->skillIds.empty())
    {
        if (skillSlotIntervalMs.size() != ai->skillIds.size())
            skillSlotIntervalMs.assign(ai->skillIds.size(), 0);

        struct Cand
        {
            size_t index;
            int32_t skillId;
            int32_t prio;
        };
        std::vector<Cand> cands;
        cands.reserve(ai->skillIds.size());

        const size_t n = ai->skillIds.size();
        for (size_t i = 0; i < n; ++i)
        {
            if (i < skillSlotIntervalMs.size() && skillSlotIntervalMs[i] > 0)
                continue;
            const int32_t sid = ai->skillIds[i];
            if (sid <= 0)
                continue;
            if (!mgr->isAllowCast(entity, sid, true))
                continue;

            int32_t prio = 1;
            if (i < ai->skillPriorityLevel.size())
                prio = ai->skillPriorityLevel[i];
            cands.push_back(Cand{i, sid, prio});
        }

        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
            if (a.prio != b.prio)
                return a.prio > b.prio;
            return a.index < b.index;
        });

        for (const Cand& c : cands)
        {
            const int32_t skillAiId = (c.index < ai->skillAiIds.size()) ? ai->skillAiIds[c.index] : 0;
            if (skillAiId > 0)
            {
                SkillAi* st = skillAiForSlot(c.index);
                if (!st || !st->check(entity, player, rng))
                    continue;
            }

            int32_t step = 0;
            int32_t slot = mgr->findSlotForSkill(entity, c.skillId, &step);
            if (slot <= 0)
                slot = static_cast<int32_t>(INPUT_SLOT_0);
            if (!mgr->presetSkill(entity, c.skillId, slot, step))
                continue;
            skillId  = c.skillId;
            usedPrio = c.prio;
            break;
        }

        if (skillId > 0)
        {
            const size_t nSlots = ai->skillIds.size();
            for (size_t j = 0; j < nSlots; ++j)
            {
                int32_t prio = 1;
                if (j < ai->skillPriorityLevel.size())
                    prio = ai->skillPriorityLevel[j];
                if (prio != usedPrio)
                    continue;
                int32_t cd = ai->skillInterval;
                if (j < ai->skillPriorityLevelCd.size() && ai->skillPriorityLevelCd[j] > 0)
                    cd = ai->skillPriorityLevelCd[j];
                if (cd <= 0)
                    cd = 1200;
                if (j < skillSlotIntervalMs.size())
                    skillSlotIntervalMs[j] = cd;
            }
            return;
        }
        return;
    }

    if (!deck->skills.empty())
        skillId = deck->skills.front().skillAttackId;

    if (skillId > 0 && mgr->isAllowCast(entity, skillId, true))
    {
        int32_t step = 0;
        int32_t slot = mgr->findSlotForSkill(entity, skillId, &step);
        if (slot <= 0)
            slot = static_cast<int32_t>(INPUT_SLOT_0);
        mgr->presetSkill(entity, skillId, slot, step);
    }
}

SkillAi* AiAgent::findSkillAi(int32_t id) const
{
    for (const auto& st : skillAis)
    {
        if (st && st->skillAiId == id)
            return st.get();
    }
    return nullptr;
}

SkillAi* AiAgent::skillAiForSlot(size_t slot) const
{
    const AiConfig* ai = config();
    if (!ai || slot >= ai->skillAiIds.size() || ai->skillAiIds[slot] <= 0)
        return nullptr;
    size_t k = 0;
    for (size_t i = 0; i < slot; ++i)
    {
        if (ai->skillAiIds[i] > 0)
            ++k;
    }
    if (k >= skillAis.size())
        return nullptr;
    return skillAis[k].get();
}

const AiConfig* AiAgent::config() const
{
    return loadAiConfig(aiConfigId);
}

AiAgent* AiAgent::of(Entity* entity)
{
    if (!entity)
        return nullptr;
    auto* comp = MG_GET_COMPONENT(entity, AIComponent);
    return comp ? comp->ensureAgent() : nullptr;
}

Entity* AiAgent::findNearestPlayer(ECSManager* ecs, const TransformComponent* selfTf, bool skipDead)
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
        if (skipDead)
        {
            if (auto* attr = MG_GET_COMPONENT(e, AttributeComponent))
            {
                if (attr->hp <= 0.0f)
                    continue;
            }
        }
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

Random& AiAgent::worldRandom(ECSManager* ecs)
{
    static Random fallback;
    if (!ecs)
        return fallback;
    if (auto* word = reinterpret_cast<GameWord*>(ecs->getUserdata()))
        return word->random;
    return fallback;
}

const AiConfig* AiAgent::resolveConfig(Entity* entity)
{
    if (!entity)
        return nullptr;
    auto* behavior         = MG_GET_COMPONENT(entity, BehaviorComponent);
    const RoleConfig* role = behavior ? behavior->roleConfig : nullptr;
    if (!role)
    {
        if (auto* avatar = MG_GET_COMPONENT(entity, AvatarComponent))
        {
            role = avatar->roleConfig;
            if (!role && avatar->roleId > 0)
                role = Config::getInstance()->getRoleConfigById(avatar->roleId);
            if (role && behavior)
                behavior->roleConfig = role;
        }
    }
    if (!role || role->aiIds.empty())
        return nullptr;
    return Config::getInstance()->getAiConfigById(role->aiIds.front());
}

void AiAgent::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeUint16(static_cast<uint16_t>(skillAis.size()));
    for (const auto& st : skillAis)
    {
        MG_ASSERT(st && "AiAgent: null SkillAi");
        st->serialize(byteBuffer);
    }
}

bool AiAgent::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    uint16_t count = 0;
    if (!byteBuffer.getUint16(count))
        return false;
    if (count != static_cast<uint16_t>(skillAis.size()))
    {
        MG_LOG_E("AiAgent: skillAi count mismatch expect={} got={}", skillAis.size(), count);
        MG_ASSERT(false);
        return false;
    }
    for (auto& st : skillAis)
    {
        MG_ASSERT(st && "AiAgent: null SkillAi");
        const int32_t expectId = st->skillAiId;
        if (!st->deserialize(byteBuffer))
            return false;
        if (st->skillAiId != expectId)
        {
            MG_LOG_E("AiAgent: skillAi id mismatch expect={} got={}", expectId, st->skillAiId);
            MG_ASSERT(false);
            return false;
        }
    }
    return true;
}

NS_MG_END
