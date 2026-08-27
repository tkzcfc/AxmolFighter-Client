#include "AISystem.h"

#include "mugen/Components.h"
#include "mugen/ai/AiAgent.h"

NS_MG_BEGIN

namespace
{

// 有 AIComponent 则 ensure；怪物缺失时补上，再 bind 表并填快照
void prepareAgent(Entity* entity)
{
    auto* identity = MG_GET_COMPONENT(entity, IdentityComponent);
    auto* aiComp   = MG_GET_COMPONENT(entity, AIComponent);
    if (!aiComp && identity && identity->category == EntityCategory::kMonster)
        aiComp = MG_ADD_COMPONENT(entity, AIComponent);
    if (!aiComp)
        return;
    auto* agent = aiComp->ensureAgent();
    if (identity && identity->category == EntityCategory::kMonster)
        agent->bindConfig(entity);
    aiComp->restoreRuntimeData();
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

void AISystem::onEntityAdded(Entity* entity)
{
    prepareAgent(entity);
}

void AISystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    for (Entity* entity : entities)
    {
        auto* identity = MG_GET_COMPONENT(entity, IdentityComponent);
        if (!identity || identity->category != EntityCategory::kMonster)
            continue;
        prepareAgent(entity);
        if (auto* agent = AiAgent::of(entity))
            agent->update(entity, dtMs);
    }
}

NS_MG_END
