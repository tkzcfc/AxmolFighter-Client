#include "mugen/buff/Buff.h"

#include "mugen/buff/BuffManager.h"
#include "mugen/buff/BuffRuleBase.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/core/StdC.h"
#include "mugen/core/ecs/Entity.h"

#include <algorithm>

NS_MG_BEGIN

void Buff::tick(Entity* entity, int32_t dtMs)
{
    if (dtMs <= 0 || destroyed)
        return;

    if (innerCdMs > 0)
        innerCdMs = (std::max)(0, innerCdMs - dtMs);

    bool handled = false;
    if (auto* rule = BuffManager::resolveRule(*this))
        handled = rule->onTick(entity, *this, dtMs);

    if (!handled)
    {
        const auto* cfg        = Config::getInstance()->getBuffConfigById(buffId);
        const int32_t interval = BuffRuleUtil::intervalMs(cfg);
        if (cfg && interval > 0)
        {
            tickAccumMs += dtMs;
            if (tickAccumMs >= interval)
            {
                tickAccumMs       = 0;
                const float delta = cfg->paramValue.empty() ? 0.0f : cfg->paramValue[0];
                BuffRuleUtil::applyHpDelta(entity, delta * static_cast<float>((std::max)(1, repeatCount)));
            }
        }
    }

    if (remainingMs > 0)
    {
        remainingMs -= dtMs;
        if (remainingMs <= 0)
            destroyed = true;
    }
}

NS_MG_END
