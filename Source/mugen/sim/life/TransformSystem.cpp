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
            link.members = members;
            link.index   = i;
        }
        else
        {
            TransformLinkComponent link;
            link.members = members;
            link.index   = i;
            world.ecs().add(member, link);
        }
    }
}
}  // namespace

// 系统 TransformSystem：9.2 行，见 DESIGN.md §3.2。按请求生成新形态并休眠旧形态。
void runTransform(World& world)
{
    auto& requests = world.ecs().singleton<EventQueue<TransformRequest>>().items;
    auto& ecs      = world.ecs();
    for (const TransformRequest& request : requests)
    {
        const Entity old = request.entity;
        if (!ecs.alive(old) || ecs.has<InactiveComponent>(old) || ecs.has<DespawningComponent>(old))
            continue;
        if (ecs.has<HealthComponent>(old) && ecs.get<HealthComponent>(old).dead)
            continue;
        if (ecs.has<RoleStateComponent>(old) && ecs.get<RoleStateComponent>(old).state == RoleStateId::Death)
            continue;
        const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(request.targetRoleId);
        if (!row)
            continue;
        const auto& oldId        = ecs.get<IdentityComponent>(old);
        const auto& oldTransform = ecs.get<TransformComponent>(old);
        SpawnRequest spawn;
        spawn.kind         = SpawnKind::Role;
        spawn.configId     = request.targetRoleId;
        spawn.owner        = oldId.owner;
        spawn.position     = oldTransform.position;
        spawn.towardX      = oldTransform.towardX;
        spawn.vectorX      = oldTransform.vectorX;
        spawn.vectorZ      = oldTransform.vectorZ;
        spawn.level        = oldId.level;
        spawn.camp         = oldId.camp;
        spawn.isAi         = !oldId.playerSlot.has_value();
        if (oldId.playerSlot)
        {
            spawn.hasPlayerSlot = true;
            spawn.playerSlot    = *oldId.playerSlot;
        }
        const Entity neu = life::rules::materialize(world, spawn, SpawnStage::Lifecycle);
        if (neu.value == 0)
            continue;
        if (request.type == 0 && ecs.has<HealthComponent>(old) && ecs.has<HealthComponent>(neu))
        {
            const auto& src = ecs.get<HealthComponent>(old);
            auto& dst       = ecs.get<HealthComponent>(neu);
            dst.maxHp     = src.maxHp;
            dst.hp        = src.hp;
            dst.prevHp    = src.hp;
        }
        life::rules::inheritBuffs(world, old, neu, 2);
        if (ecs.has<PlayerControlComponent>(old))
            ecs.remove<PlayerControlComponent>(old);
        ecs.add(old, InactiveComponent{});
        std::vector<Entity> members;
        if (ecs.has<TransformLinkComponent>(old))
            members = ecs.get<TransformLinkComponent>(old).members;
        else
            members.push_back(old);
        members.push_back(neu);
        writeLink(world, members);
        SpawnEvent spawned;
        spawned.entity   = neu;
        spawned.kind     = SpawnKind::Role;
        spawned.configId = request.targetRoleId;
        spawned.owner    = oldId.owner;
        spawned.ordinal  = ecs.get<IdentityComponent>(neu).ordinal;
        ecs.singleton<EventQueue<SpawnEvent>>().push(spawned);
        CueEvent cue;
        cue.entity = neu;
        cue.kind   = CueKind::TransformSwap;
        cue.id     = request.targetRoleId;
        ecs.singleton<EventQueue<CueEvent>>().push(cue);
    }
    requests.clear();
}
}  // namespace mugen::sim
