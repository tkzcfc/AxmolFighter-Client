#include "BuffSystem.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"

#include <algorithm>

NS_MG_BEGIN

BuffSystem::BuffSystem() {}
BuffSystem::~BuffSystem() {}

void BuffSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, BuffComponent);
}

void BuffSystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    for (Entity* entity : entities)
    {
        auto* buffComp = MG_GET_COMPONENT(entity, BuffComponent);
        auto* attr     = MG_GET_COMPONENT(entity, AttributeComponent);
        if (!buffComp)
            continue;

        for (auto it = buffComp->buffs.begin(); it != buffComp->buffs.end();)
        {
            const auto* cfg = Config::getInstance()->getBuffConfigById(it->buffId);
            if (cfg && cfg->interval > 0 && attr)
            {
                it->tickAccumMs += dtMs;
                if (it->tickAccumMs >= cfg->interval)
                {
                    it->tickAccumMs           = 0;
                    attr->currentAttribute.hp = std::max(0.0f, attr->currentAttribute.hp + cfg->paramValue);
                }
            }

            if (it->remainingMs > 0)
            {
                it->remainingMs -= dtMs;
                if (it->remainingMs <= 0)
                {
                    it = buffComp->buffs.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
}

NS_MG_END
