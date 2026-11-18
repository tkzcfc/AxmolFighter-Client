// [11.1] PresentationSystem
// 读：全部事实队列
// 写：`PresentationEvents`
// 消费：事实（只读）
// 产生：`PresentationEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/battle/BuffDef.h"
#include "mugen/conf/battle/RoleDef.h"
#include "mugen/conf/tables/ActionTables.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mugen::sim
{
namespace
{
// 追加一条表现事件：帧内 ordinal 从 0 计，seq 全局单调。
void emit(World& world, Entity entity, uint64_t& ordinal, PresentationPayload payload)
{
    auto& out = world.ecs().singleton<PresentationEvents>();
    out.events.push_back(PresentationEvent{
        world.ecs().singleton<WorldClock>().frame,
        entity,
        ordinal++,
        out.nextSeq++,
        std::move(payload),
    });
}

// 按阵营累加玩家英雄生命比例，供决斗结束漂字使用。
std::map<int32_t, Fixed> campHpRatios(World& world)
{
    std::map<int32_t, Fixed> hpSum;
    std::map<int32_t, Fixed> maxHpSum;
    world.ecs().each<IdentityComponent, RoleStateComponent, HealthComponent>(
        [&](Entity entity, const IdentityComponent& identity, const RoleStateComponent&, const HealthComponent& health) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (identity.roleType != 1 || !identity.playerSlot)
                return;
            hpSum[identity.camp] += health.hp;
            maxHpSum[identity.camp] += health.maxHp;
        });
    std::map<int32_t, Fixed> ratios;
    for (const auto& [camp, hp] : hpSum)
        ratios[camp] = maxHpSum[camp] > Fixed{} ? hp / maxHpSum[camp] : Fixed{};
    return ratios;
}

// 命中漂字的屏幕平面接触点（横向 + 高度加纵深，再叠加受击者偏移）。
FixedVec2 hurtContact(World& world, const HitEvent& ev)
{
    FixedVec2 screen{ev.contact.x, ev.contact.y + ev.contact.z};
    const auto* identity = world.ecs().tryGet<IdentityComponent>(ev.target);
    if (!identity || identity->kind != SpawnKind::Role)
        return screen;
    const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(identity->configId);
    if (!row || row->hurtNumPos.size() < 2)
        return screen;
    Fixed vectorX = Fixed::fromInt(1);
    if (const auto* xf = world.ecs().tryGet<TransformComponent>(ev.effect))
        vectorX = xf->vectorX;
    screen.x -= vectorX * Fixed::fromInt(row->hurtNumPos[0]);
    screen.y += Fixed::fromInt(row->hurtNumPos[1]);
    return screen;
}
}  // namespace

// 系统 PresentationSystem：11.1 行，见 DESIGN.md §3.2。按固定顺序把事实队列译成表现事件。
void runPresentation(World& world)
{
    uint64_t ordinal = 0;
    auto& ecs        = world.ecs();

    for (const SpawnEvent& ev : ecs.singleton<EventQueue<SpawnEvent>>().items)
        emit(world, ev.entity, ordinal,
             EntitySpawned{static_cast<PresentEntityKind>(ev.kind), ev.configId});

    for (const StateEvent& ev : ecs.singleton<EventQueue<StateEvent>>().items)
    {
        const auto from   = static_cast<RoleStateId>(ev.from);
        const auto to     = static_cast<RoleStateId>(ev.to);
        const bool wasHit = role::isHitState(from);
        const bool nowHit = role::isHitState(to);
        if (!wasHit && nowHit)
            emit(world, ev.entity, ordinal, RoleIntensity{"1.3"_fx});
        else if (wasHit && !nowHit)
            emit(world, ev.entity, ordinal, RoleIntensity{"1"_fx});
        if (nowHit && to != RoleStateId::Death)
        {
            const auto* identity = ecs.tryGet<IdentityComponent>(ev.entity);
            if (identity && identity->kind == SpawnKind::Role)
            {
                const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(identity->configId);
                if (row && !row->soundId.empty())
                {
                    std::vector<int32_t> sounds;
                    for (const int32_t id : row->soundId)
                        if (id > 0)
                            sounds.push_back(id);
                    if (!sounds.empty())
                        emit(world, ev.entity, ordinal, Sound{0, std::move(sounds), false, -1});
                }
            }
        }
    }

    for (const HitEvent& ev : ecs.singleton<EventQueue<HitEvent>>().items)
    {
        if (ev.result == HitResult::Miss)
            emit(world, ev.target, ordinal, Tips{"MISS"});
        else if (ev.result == HitResult::Evade)
            emit(world, ev.target, ordinal, Tips{"DODGE"});
        else if (ev.result == HitResult::Hit && ev.damage > Fixed{})
            emit(world, ev.target, ordinal,
                 HurtNumber{ev.damage.toInt(), ev.critical, true, hurtContact(world, ev)});
    }

    for (const HpEvent& ev : ecs.singleton<EventQueue<HpEvent>>().items)
    {
        if (ev.cause != HpCause::Dot && ev.cause != HpCause::Heal)
            continue;
        if (ev.delta != Fixed{})
            emit(world, ev.entity, ordinal, HurtNumber{(-ev.delta).toInt(), false, false, {}});
    }

    for (const BuffEvent& ev : ecs.singleton<EventQueue<BuffEvent>>().items)
    {
        const auto* def = conf::Config::instance().find<conf::battle::BuffDef>(ev.buffId);
        if (!def || def->presentation.spineId <= 0)
            continue;
        const int32_t spineId       = def->presentation.spineId;
        const auto& step            = def->presentation.spineStep;
        const bool hasAppear        = !step.empty() && step[0] == 1;
        const bool hasDisappear     = step.size() >= 3 && step[2] == 1;
        if (ev.phase == BuffPhase::Added)
            emit(world, ev.entity, ordinal, BuffSpine{ev.serial, spineId, hasAppear ? 1 : 2, ev.buffId});
        else if (ev.phase == BuffPhase::Removed)
            emit(world, ev.entity, ordinal, BuffSpine{ev.serial, spineId, hasDisappear ? 3 : 0, ev.buffId});
    }

    for (const CueEvent& ev : ecs.singleton<EventQueue<CueEvent>>().items)
    {
        switch (ev.kind)
        {
        case CueKind::Sound:
            emit(world, ev.entity, ordinal, Sound{ev.id, ev.candidates, ev.stopLoop, ev.param});
            break;
        case CueKind::ShakeScreen:
            if (const auto* cam = conf::Config::instance().find<conf::CameraConfig>(ev.id))
                emit(world, ev.entity, ordinal,
                     CameraShake{Fixed::fromInt(cam->amplitudeY), Fixed::fromInt(cam->duration), ev.id});
            break;
        case CueKind::AfterImage:
            emit(world, ev.entity, ordinal, Ghost{ev.param != 0 ? "-1"_fx : Fixed{}});
            break;
        case CueKind::Tip:
            emit(world, ev.entity, ordinal, Tips{std::to_string(ev.id)});
            break;
        case CueKind::Dialogue:
            emit(world, ev.entity, ordinal, Bubble{ev.id});
            break;
        case CueKind::FullScreen:
            emit(world, ev.entity, ordinal, DisplaySpine{ev.id, ev.groups});
            break;
        case CueKind::WhiteFrame:
            emit(world, ev.entity, ordinal, WhiteFrame{ev.param != 0});
            break;
        case CueKind::StealthView:
            emit(world, ev.entity, ordinal, StealthView{ev.param != 0});
            break;
        case CueKind::SuperArmorOutline:
            emit(world, ev.entity, ordinal, SuperArmorBorder{ev.param != 0});
            break;
        case CueKind::TransformSwap:
            emit(world, ev.entity, ordinal, TransformSwap{ev.entity, Entity{static_cast<uint32_t>(ev.param)}});
            break;
        case CueKind::PartnerRage:
            emit(world, ev.entity, ordinal, PartnerRageChanged{ev.param != 0});
            break;
        case CueKind::InkScreen:
            break;
        }
    }

    for (const DespawnEvent& ev : ecs.singleton<EventQueue<DespawnEvent>>().items)
    {
        if (ev.kind == SpawnKind::Effect && ev.clipId != 0)
            emit(world, ev.entity, ordinal, Sound{0, {}, true, ev.clipId});
        if (ev.kind == SpawnKind::Role && ev.reason == DespawnReason::DeathEnd)
            emit(world, ev.entity, ordinal, RoleDeath{ev.configId});
        emit(world, ev.entity, ordinal,
             EntityDespawned{static_cast<PresentEntityKind>(ev.kind), ev.configId});
    }

    for (const RoomEvent& ev : ecs.singleton<EventQueue<RoomEvent>>().items)
    {
        switch (ev.kind)
        {
        case RoomEventKind::StateChanged:
            emit(world, ev.entity, ordinal, RoomStateChanged{ev.payload});
            break;
        case RoomEventKind::Victory:
            emit(world, ev.entity, ordinal, Victory{ev.payload});
            break;
        case RoomEventKind::Failure:
            emit(world, ev.entity, ordinal, Failure{ev.payload});
            break;
        case RoomEventKind::Warning:
            emit(world, ev.entity, ordinal, RoomWarning{ev.payload, ev.param != 0});
            break;
        case RoomEventKind::PvpEnded:
            emit(world, ev.entity, ordinal,
                 PvpRoundEnd{ev.payload, static_cast<PvpRoundEndReason>(ev.param), campHpRatios(world)});
            break;
        }
    }
}
}  // namespace mugen::sim
