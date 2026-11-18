#include "mugen/sim/view/DebugView.h"

#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/sim/skill/SkillRules.h"

namespace mugen::sim::view
{
namespace
{
// 待回收或休眠的实体对调试层不可见。
bool hidden(const WorldECS& ecs, Entity entity)
{
    return ecs.has<DespawningComponent>(entity) || ecs.has<InactiveComponent>(entity);
}

// 拷贝世界轴对齐盒到调试框。
DebugBox toDebugBox(const WorldBox& box)
{
    return {box.pos, box.size};
}
}  // namespace

// 当前逻辑帧号。
uint64_t debugFrame(const World& world)
{
    return world.frame();
}

// 当前世界哈希。
uint64_t debugHash(const World& world)
{
    return world.hash();
}

// 收集当前碰撞框，跳过待回收与休眠实体。
void collectColliders(const World& world, std::vector<ColliderDebugView>& out)
{
    out.clear();
    const auto& ecs = world.ecs();
    ecs.each<HitboxesComponent>([&](Entity entity, const HitboxesComponent& hitboxes) {
        if (hidden(ecs, entity))
            return;
        ColliderDebugView view;
        view.entity = entity;
        view.radius = hitboxes.radius;
        if (const auto* transform = ecs.tryGet<TransformComponent>(entity))
            view.position = transform->position;
        view.attack.reserve(hitboxes.attack.size());
        for (const auto& box : hitboxes.attack)
            view.attack.push_back(toDebugBox(box));
        view.damage.reserve(hitboxes.damage.size());
        for (const auto& box : hitboxes.damage)
            view.damage.push_back(toDebugBox(box));
        out.push_back(std::move(view));
    });
}

// 收集地图可移动区域。
void moveRanges(const World& world, std::vector<MapMoveRangeView>& out)
{
    out.clear();
    const auto& ranges = world.ecs().singleton<MapRegion>().moveRanges;
    out.reserve(ranges.size());
    for (const auto& range : ranges)
        out.push_back({range.x, range.y, range.width, range.height});
}

// 收集角色调试快照，跳过待回收与休眠实体。
void collectRoleDebug(const World& world, std::vector<RoleDebugView>& out)
{
    out.clear();
    const auto& ecs = world.ecs();
    ecs.each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (hidden(ecs, entity))
            return;
        const bool summon = ecs.has<SummonComponent>(entity) ||
                            identity.roleType == static_cast<int32_t>(EntityRoleType::kSummon);
        if (identity.kind != SpawnKind::Role && !summon)
            return;
        const auto* state     = ecs.tryGet<RoleStateComponent>(entity);
        const auto* transform = ecs.tryGet<TransformComponent>(entity);
        if (!state || !transform)
            return;
        RoleDebugView view;
        view.entity   = entity;
        view.roleId   = identity.configId;
        view.level    = identity.level;
        view.state    = static_cast<int32_t>(state->state);
        view.position = transform->position;
        out.push_back(view);
    });
}

// 调试面板 Cast：对空闲或可移动角色按槽位开始施法。
bool debugCastSlot(World& world, Entity player, std::string_view poolSlot)
{
    auto& ecs = world.ecs();
    if (!ecs.alive(player) || hidden(ecs, player) || !ecs.has<SkillBookComponent>(player))
        return false;
    const auto* state = ecs.tryGet<RoleStateComponent>(player);
    if (!state)
        return false;
    if (state->state != RoleStateId::Idle && !role::isMoveState(state->state))
        return false;
    const int32_t next = skill::rules::getFightSkill(world, player, poolSlot);
    if (next == kNoSkill || !skill::rules::isAllowCast(world, player, next))
        return false;
    skill::rules::beginCast(world, player, next);
    return true;
}
}  // namespace mugen::sim::view
