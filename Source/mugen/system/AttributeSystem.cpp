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
    auto attributeComp = MG_GET_COMPONENT(entity, AttributeComponent);
    attributeComp->bindVariable();

    auto transformComp = MG_GET_COMPONENT(entity, TransformComponent);
    if (transformComp)
    {
        attributeComp->setFacingDirection(transformComp->facingDirection);
    }
}

void AttributeSystem::onEntityRemoved(Entity* entity) {}

void AttributeSystem::update()
{
    for (auto entity : entities)
    {
        auto attributeComp = MG_GET_COMPONENT(entity, AttributeComponent);
        attributeComp->bindVariable();
    }
}

NS_MG_END
