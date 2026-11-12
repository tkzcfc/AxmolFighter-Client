// [9.2] TransformSystem
// 读：`TransformRequest`、`IdentityComponent`、`TransformComponent`、`HealthComponent`、`PlayerSlots`
// 写：新形态实体、旧形态加 `InactiveComponent`、`TransformLinkComponent`、`PlayerSlots`
// 消费：`TransformRequest`
// 产生：`SpawnEvent`、`CueEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 把成员表写到组内每个形态上。
void writeLink(World& world, const std::vector<Entity>& members)
{
    for (int32_t i = 0; i < static_cast<int32_t>(members.size()); ++i)
    {
        const Entity member = members[static_cast<size_t>(i)];
        if (!world.ecs().alive(member))
            continue;
        if (world.ecs().has<TransformLinkComponent>(member))
        {
            auto& link     = world.ecs().get<TransformLinkComponent>(member);
            link.m_members = members;
            link.m_index   = i;
        }
        else
        {
            TransformLinkComponent link;
            link.m_members = members;
            link.m_index   = i;
            world.ecs().add(member, link);
        }
    }
}
}  // namespace

// 系统 TransformSystem：9.2 行，见 DESIGN.md §3.2。按请求生成新形态并休眠旧形态。
void runTransform(World& world)
{
    auto& requests = world.ecs().singleton<EventQueue<TransformRequest>>().m_items;
    auto& ecs      = world.ecs();
    for (const TransformRequest& request : requests)
    {
        const Entity old = request.m_entity;
        if (!ecs.alive(old) || ecs.has<InactiveComponent>(old) || ecs.has<DespawningComponent>(old))
            continue;
        if (ecs.has<HealthComponent>(old) && ecs.get<HealthComponent>(old).m_dead)
            continue;
        if (ecs.has<RoleStateComponent>(old) && ecs.get<RoleStateComponent>(old).m_state == RoleStateId::Death)
            continue;
        const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(request.m_targetRoleId);
        if (!row)
            continue;
        const auto& oldId        = ecs.get<IdentityComponent>(old);
        const auto& oldTransform = ecs.get<TransformComponent>(old);
        SpawnRequest spawn;
        spawn.m_kind         = SpawnKind::Role;
        spawn.m_configId     = request.m_targetRoleId;
        spawn.m_owner        = oldId.m_owner;
        spawn.m_position     = oldTransform.m_position;
        spawn.m_towardX      = oldTransform.m_towardX;
        spawn.m_vectorX      = oldTransform.m_vectorX;
        spawn.m_vectorZ      = oldTransform.m_vectorZ;
        spawn.m_level        = oldId.m_level;
        spawn.m_camp         = oldId.m_camp;
        spawn.m_isAi         = !oldId.m_playerSlot.has_value();
        if (oldId.m_playerSlot)
        {
            spawn.m_hasPlayerSlot = true;
            spawn.m_playerSlot    = *oldId.m_playerSlot;
        }
        const Entity neu = life::rules::materialize(world, spawn, SpawnStage::Lifecycle);
        if (neu.value == 0)
            continue;
        if (request.m_type == 0 && ecs.has<HealthComponent>(old) && ecs.has<HealthComponent>(neu))
        {
            const auto& src = ecs.get<HealthComponent>(old);
            auto& dst       = ecs.get<HealthComponent>(neu);
            dst.m_maxHp     = src.m_maxHp;
            dst.m_hp        = src.m_hp;
            dst.m_prevHp    = src.m_hp;
        }
        life::rules::inheritBuffs(world, old, neu, 2);
        if (ecs.has<PlayerControlComponent>(old))
            ecs.remove<PlayerControlComponent>(old);
        ecs.add(old, InactiveComponent{});
        std::vector<Entity> members;
        if (ecs.has<TransformLinkComponent>(old))
            members = ecs.get<TransformLinkComponent>(old).m_members;
        else
            members.push_back(old);
        members.push_back(neu);
        writeLink(world, members);
        SpawnEvent spawned;
        spawned.m_entity   = neu;
        spawned.m_kind     = SpawnKind::Role;
        spawned.m_configId = request.m_targetRoleId;
        spawned.m_owner    = oldId.m_owner;
        spawned.m_ordinal  = ecs.get<IdentityComponent>(neu).m_ordinal;
        ecs.singleton<EventQueue<SpawnEvent>>().push(spawned);
        CueEvent cue;
        cue.m_entity = neu;
        cue.m_kind   = CueKind::TransformSwap;
        cue.m_id     = request.m_targetRoleId;
        ecs.singleton<EventQueue<CueEvent>>().push(cue);
    }
    requests.clear();
}
}  // namespace mugen::sim
