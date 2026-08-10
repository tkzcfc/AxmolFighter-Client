#include "BuffSystem.h"

#include "mugen/buff/BuffApi.h"
#include "mugen/buff/BuffRuleFactory.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/core/ecs/Entity.h"

#include <algorithm>

NS_MG_BEGIN

BuffSystem::BuffSystem() {}
BuffSystem::~BuffSystem() {}

void BuffSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, BuffComponent);
    BuffRuleFactory::instance().registerBuiltinRules();
}

void BuffSystem::update()
{
    const int32_t dtMs = getECSManager()->getLastUpdateTimeMs();
    for (Entity* entity : entities)
    {
        auto* buffComp = MG_GET_COMPONENT(entity, BuffComponent);
        if (!buffComp)
            continue;

        for (auto it = buffComp->buffs.begin(); it != buffComp->buffs.end();)
        {
            if (it->innerCdMs > 0)
                it->innerCdMs = (std::max)(0, it->innerCdMs - dtMs);

            bool handled = false;
            if (auto* rule = BuffApi::resolveRule(*it))
                handled = rule->onTick(entity, *it, dtMs);

            // 无规则或规则未处理周期：保留旧默认周期伤逻辑
            if (!handled)
            {
                const auto* cfg = Config::getInstance()->getBuffConfigById(it->buffId);
                auto* attr      = MG_GET_COMPONENT(entity, AttributeComponent);
                if (cfg && cfg->interval > 0 && attr)
                {
                    it->tickAccumMs += dtMs;
                    if (it->tickAccumMs >= cfg->interval)
                    {
                        it->tickAccumMs = 0;
                        const float delta = cfg->paramValue.empty() ? 0.0f : cfg->paramValue[0];
                        attr->currentAttribute.hp =
                            (std::max)(0.0f, attr->currentAttribute.hp + delta * static_cast<float>((std::max)(1, it->repeatCount)));
                    }
                }
            }

            if (it->remainingMs > 0)
            {
                it->remainingMs -= dtMs;
                if (it->remainingMs <= 0)
                {
                    if (auto* rule = BuffApi::resolveRule(*it))
                        rule->onRemove(entity, *it);
                    BuffRuleUtil::detachSpine(entity, *it);
                    it = buffComp->buffs.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
}

NS_MG_END
