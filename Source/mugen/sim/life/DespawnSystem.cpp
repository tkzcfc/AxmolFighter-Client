// [9.3] DespawnSystem
// 读：`DespawningComponent`、`EffectInfoComponent`、`LifetimeComponent`、`TransformLinkComponent`
// 写：`DespawningComponent`
// 消费：`DespawnRequest`
// 产生：`DespawnEvent`、`SpawnRequest`(后继特效)、`DisplacementEvent`(Ended)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/battle/ClipDef.h"
#include <vector>

namespace mugen::sim
{
// 系统 DespawnSystem：9.3 行，见 DESIGN.md §3.2。回收特效并只在允许时派生后继。
void runDespawn(World& world)
{
    auto& ecs      = world.ecs();
    auto& requests = ecs.singleton<EventQueue<DespawnRequest>>().m_items;
    std::vector<DespawnRequest> pending = requests;
    // 变身组任一形态回收时其余形态一并回收。
    for (size_t i = 0; i < pending.size(); ++i)
    {
        const Entity entity = pending[i].m_entity;
        if (!ecs.alive(entity) || !ecs.has<TransformLinkComponent>(entity))
            continue;
        const auto members = ecs.get<TransformLinkComponent>(entity).m_members;
        for (const Entity member : members)
        {
            if (member.value == entity.value || !ecs.alive(member) || ecs.has<DespawningComponent>(member))
                continue;
            life::rules::despawn(world, member, pending[i].m_reason);
            pending.push_back({member, pending[i].m_reason});
        }
    }
    for (const DespawnRequest& request : pending)
    {
        const Entity entity = request.m_entity;
        if (!ecs.alive(entity))
            continue;
        if (const auto* info = ecs.tryGet<EffectInfoComponent>(entity))
        {
            const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(info->m_configId);
            const bool blocked = ecs.has<DisplacementComponent>(entity) && ecs.get<DisplacementComponent>(entity).m_wall &&
                                 ecs.get<DisplacementComponent>(entity).m_wallPolicy ==
                                     static_cast<int32_t>(conf::battle::ClipWallPolicy::EndClip);
            // 只有仍允许后继（autoRelease=3 或片段正常结束）且未撞墙结束时派生。
            if (info->m_next && !blocked && row && row->nextEffectId > 0 && ecs.alive(info->m_owner))
                effect::rules::spawnChild(world, entity, row->nextEffectId);
        }
        if (ecs.has<BuffListComponent>(entity))
        {
            std::vector<int32_t> ids;
            for (const auto& inst : ecs.get<BuffListComponent>(entity).m_instances)
                if (!inst.m_removing)
                    ids.push_back(inst.m_id);
            for (const int32_t id : ids)
                buff::rules::remove(world, entity, id);
        }
        if (ecs.has<DisplacementComponent>(entity))
            displacement::rules::reset(world, entity);
        DespawnEvent event;
        event.m_entity = entity;
        event.m_reason = request.m_reason;
        ecs.singleton<EventQueue<DespawnEvent>>().push(event);
        ecs.destroy(entity);
    }
}
}  // namespace mugen::sim
