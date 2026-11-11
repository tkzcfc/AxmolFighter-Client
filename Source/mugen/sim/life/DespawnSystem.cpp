// [9.3] DespawnSystem
// 读：`Despawning`、`EffectInfo`、`Lifetime`
// 写：`Despawning`
// 消费：`DespawnRequest`
// 产生：`DespawnEvent`、`SpawnRequest`(后继特效)、`DisplacementEvent`(Ended)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include <vector>
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
                effect::rules::spawnChild(world, entity, row->nextEffectId);
        }
        if (ecs.has<BuffList>(entity))
        {
            std::vector<int32_t> ids;
            for (const auto& inst : ecs.get<BuffList>(entity).m_instances)
                if (!inst.m_removing)
                    ids.push_back(inst.m_id);
            for (const int32_t id : ids)
                buff::rules::remove(world, entity, id);
        }
        // 曲线还没耗尽就被回收时，按当时位置结束位移。
        if (ecs.has<Displacement>(entity))
            displacement::rules::reset(world, entity);
        DespawnEvent event;
        event.m_entity = entity;
        event.m_reason = request.m_reason;
        ecs.singleton<EventQueue<DespawnEvent>>().push(event);
        ecs.destroy(entity);
    }
}
}  // namespace mugen::sim
