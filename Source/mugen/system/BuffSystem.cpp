#include "BuffSystem.h"

#include "mugen/Components.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/buff/BuffRuleFactory.h"

NS_MG_BEGIN

namespace
{

void prepareManager(Entity* entity)
{
    auto* comp = MG_GET_COMPONENT(entity, BuffComponent);
    if (!comp)
        return;
    comp->ensureManager();
    comp->restoreRuntimeData();
}

}  // namespace

BuffSystem::BuffSystem() {}
BuffSystem::~BuffSystem() {}

void BuffSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, BuffComponent);
    BuffRuleFactory::instance().registerBuiltinRules();
}

void BuffSystem::onEntityAdded(Entity* entity)
{
    prepareManager(entity);
}

void BuffSystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    for (Entity* entity : entities)
    {
        prepareManager(entity);
        if (auto* mgr = BuffManager::of(entity))
            mgr->update(entity, dtMs);
    }
}

NS_MG_END
