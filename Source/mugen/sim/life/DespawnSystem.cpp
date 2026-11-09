// [9.3] DespawnSystem
// 读：`Despawning`、`EffectInfo`、`Lifetime`
// 写：`Despawning`
// 消费：`DespawnRequest`
// 产生：`DespawnEvent`、`SpawnRequest`(后继特效)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
// 系统 DespawnSystem：9.3 行，见 DESIGN.md §3.2。回收特效并按需派生后继特效。
void runDespawn(World& world)
{
    auto& requests = world.ecs().singleton<EventQueue<DespawnRequest>>().m_items;
    for (const DespawnRequest& request : requests)
    {
        const Entity entity = request.m_entity;
        auto& ecs           = world.ecs();
        if (!ecs.alive(entity))
            continue;
        // 派生后继特效（autoRelease=3 触发）；主人已回收时无法取出生点，跳过。
        if (const auto* info = ecs.tryGet<EffectInfo>(entity))
        {
            const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(info->m_configId);
            if (row && row->nextEffectId > 0 && ecs.alive(info->m_owner))
                effect::rules::spawnFromRole(world, info->m_owner, row->nextEffectId, {});
        }
        DespawnEvent event;
        event.m_entity = entity;
        event.m_reason = request.m_reason;
        ecs.singleton<EventQueue<DespawnEvent>>().push(event);
        ecs.destroy(entity);
    }
}
}  // namespace mugen::sim
