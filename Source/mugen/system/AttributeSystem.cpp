#include "AttributeSystem.h"
#include "mugen/Components.h"

#include <algorithm>

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
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    const float dtSec  = dtMs > 0 ? static_cast<float>(dtMs) / 1000.0f : 0.0f;

    for (auto entity : entities)
    {
        auto attributeComp = MG_GET_COMPONENT(entity, AttributeComponent);
        if (!attributeComp)
            continue;

        attributeComp->bindVariable();

        // 对齐黑月：按 mpRegenSpeed 回复 MP（单位：点/秒）
        if (dtSec > 0.0f)
        {
            const float regen = attributeComp->currentAttribute.mpRegenSpeed;
            if (regen > 0.0f)
            {
                const float mpMax = static_cast<float>(attributeComp->currentAttribute.mpMax);
                attributeComp->currentAttribute.mp =
                    std::min(mpMax, attributeComp->currentAttribute.mp + regen * dtSec);
            }
        }
    }
}

NS_MG_END
