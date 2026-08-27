#include "AttributeSystem.h"
#include "mugen/Components.h"

NS_MG_BEGIN

AttributeSystem::AttributeSystem() {}

AttributeSystem::~AttributeSystem() {}

void AttributeSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, AttributeComponent);
}

void AttributeSystem::onEntityAdded(Entity* entity)
{
}

void AttributeSystem::onEntityRemoved(Entity* entity) {}

void AttributeSystem::update()
{
}

NS_MG_END
