#include "AiAgent.h"
#include "mugen/sim/World.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/sim/status/Status.h"
#include "mugen/sim/hit/Hit.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/core/math/FixedMath.h"
#include <algorithm>

namespace mugen::sim::ai
{
namespace
{
// 定点绝对值不经过宿主浮点重载。
Fixed abs(Fixed value) { return value < Fixed{} ? -value : value; }
// 阵营组而非攻击掩码决定 AI 的候选范围。
int campGroup(int camp)
{
    switch (camp)
    {
    case 1: return 1;
    case 2: case 4: return 2;
    case 8: case 16: return 3;
    default: return 0;
    }
}
// 组暂停对应逻辑禁用，销毁请求也不再参与索敌。
bool enabled(const EntityGroup& group)
{
    return group.kind == GroupKind::Role && !group.suspended && !group.stopped && !group.destroyRequested;
}
// 存活句柄和死亡状态共同限制跨实体访问。
bool alive(World& world, Entity entity)
{
    const auto* state = world.ecs().tryGet<RoleStatus>(entity);
    return state && !state->isDeath() && !hit::isDead(world, entity);
}
// 不可命中状态不参与普通 AI 目标选择。
bool hittable(World& world, Entity entity)
{
    return alive(world, entity) && world.ecs().get<Vitals>(entity).m_beHitEnabled &&
           !world.ecs().get<ExtraState>(entity).active(mugen::ExtraState::Stealth);
}
// 获取当前角色配置，悬空引用由入口拒绝。
const conf::AiConfig* config(World& world, Entity entity)
{
    return conf::Config::instance().find<conf::AiConfig>(world.ecs().get<RoleIdentity>(entity).aiId);
}
// 缺失区间不产生随机运动。
Fixed bound(const std::vector<int32_t>& range, size_t index)
{
    return index < range.size() ? Fixed::fromInt(range[index]) : Fixed{};
}
// 判断有符号偏移是否位于闭区间。
bool contains(const std::vector<int32_t>& range, Fixed value)
{
    return range.size() >= 2 && bound(range, 0) <= value && value <= bound(range, 1);
}
// 死亡、受击和失控时不能执行主动决策动作。
bool canAct(World& world, Entity entity)
{
    const auto& state = world.ecs().get<RoleStatus>(entity);
    return !state.isDeath() && !state.isTobeHit() &&
           world.ecs().get<ExtraState>(entity).isCanControl();
}
// 先设置技能控制角，成功释放后再更新角色朝向。
void direction(World& world, Entity entity, bool apply)
{
    auto& ecs = world.ecs();
    const auto target = ecs.get<SkillPool>(entity).m_target;
    const auto* other = ecs.tryGet<Transform>(target);
    if (!other) return;
    auto& self = ecs.get<Transform>(entity);
    const Fixed x = other->position.x - self.position.x, z = other->position.z - self.position.z;
    if (x == Fixed{} && z == Fixed{}) return;
    auto angle = "90"_fx - FixedMath::atan2Deg(z, x);
    if (angle < Fixed{}) angle += "360"_fx;
    ecs.get<SkillPool>(entity).m_controlAngle = angle;
    if (apply)
    {
        if (x != Fixed{}) { self.towardX = x > Fixed{} ? 1 : -1; self.vectorX = Fixed::fromInt(self.towardX); }
        if (z != Fixed{}) self.vectorZ = z > Fixed{} ? "1"_fx : "-1"_fx;
    }
}
// 目标失效时从稳定候选数组随机重选；无目标使用自身哨兵。
Result checkAttack(World& world, Entity entity)
{
    if (world.ecs().get<RoleStatus>(entity).isDeath()) return Result::Failure;
    auto& target = world.ecs().get<SkillPool>(entity).m_target;
    const auto taunt = buff::sneerTarget(world, entity);
    if (alive(world, taunt)) { target = taunt; return Result::Success; }
    if (!hittable(world, target))
    {
        const auto enemies = candidates(world, entity);
        if (enemies.empty()) { target = entity; return Result::Failure; }
        const auto index = world.ecs().singleton<RngState>().random.randomN(0, static_cast<int32_t>(enemies.size() - 1));
        target = enemies[index];
    }
    return Result::Success;
}
// 受击结束只在目标句柄缺失时补选，不重置警戒。
Result checkHitEnd(World& world, Entity entity)
{
    if (world.ecs().get<RoleStatus>(entity).isTobeHit()) return Result::Running;
    auto& target = world.ecs().get<SkillPool>(entity).m_target;
    if (!world.ecs().tryGet<RoleIdentity>(target))
    {
        const auto enemies = candidates(world, entity);
        if (enemies.empty()) return Result::Failure;
        target = enemies[world.ecs().singleton<RngState>().random.randomN(0, static_cast<int32_t>(enemies.size() - 1))];
    }
    return Result::Success;
}
// 原始追击条件保留坐标轴为零时的特殊分支。
Result checkChase(World& world, Entity entity, const conf::AiConfig& row)
{
    const auto* target = world.ecs().tryGet<Transform>(world.ecs().get<SkillPool>(entity).m_target);
    if (!target || !canAct(world, entity)) return Result::Failure;
    if (bound(row.chaseScopeX, 0) == Fixed{} && bound(row.chaseScopeX, 1) == Fixed{} &&
        bound(row.chaseScopeZ, 0) == Fixed{} && bound(row.chaseScopeZ, 1) == Fixed{}) return Result::Failure;
    const auto self = world.ecs().get<Transform>(entity).position;
    const Fixed x = self.x - target->position.x, z = self.z - target->position.z;
    if (x == Fixed{} && z == Fixed{}) return Result::Failure;
    if (x != Fixed{} && z != Fixed{} && contains(row.chaseScopeX, abs(x)) && contains(row.chaseScopeZ, abs(z)))
        return Result::Failure;
    return Result::Success;
}
// X/Z 使用手动施放门槛，已有技能时续接原槽。
Result specialAttack(World& world, Entity entity, bool first)
{
    if (!canAct(world, entity)) return Result::Failure;
    if (!first && candidates(world, entity).empty())
    {
        world.ecs().get<SkillPool>(entity).m_target = entity;
        return Result::Running;
    }
    if (first) direction(world, entity, false);
    const auto& pool = world.ecs().get<SkillPool>(entity);
    const std::string slot = pool.m_current == kNoSkill ? (first ? "X" : "Z") :
        pool.m_slots[pool.m_skills[pool.m_current].m_address.m_slot].m_name;
    if (!skill::presetSkill(world, entity, slot, false)) return Result::Success;
    status::dealWithStatus(world, entity, EntityRoleStatus::Attack);
    if (first) direction(world, entity, true);
    return Result::Running;
}
// 二维配置的首项；缺失优先级或间隔按各自契约回退。
int32_t firstValue(const std::vector<std::vector<int32_t>>& values, size_t index, int32_t fallback)
{
    return index < values.size() && !values[index].empty() ? values[index][0] : fallback;
}
// 普通槽按优先级降序、原索引升序检查，成功后重置同优先级间隔。
Result executeAttack(World& world, Entity entity, const conf::AiConfig& row)
{
    if (candidates(world, entity).empty() || !canAct(world, entity) ||
        !world.ecs().tryGet<Transform>(world.ecs().get<SkillPool>(entity).m_target)) return Result::Failure;
    direction(world, entity, false);
    auto& pool = world.ecs().get<SkillPool>(entity);
    if (pool.m_current != kNoSkill)
    {
        const auto slot = pool.m_slots[pool.m_skills[pool.m_current].m_address.m_slot].m_name;
        if (!skill::presetSkill(world, entity, slot, true)) return Result::Success;
        status::dealWithStatus(world, entity, EntityRoleStatus::Attack);
        return Result::Running;
    }
    if (pool.m_aiSkillCastInterval.empty())
        for (const auto& values : row.skillPriorityLevelCd)
            pool.m_aiSkillCastInterval.push_back(Fixed::fromInt(values.empty() ? row.skillInterval : values[0]));
    std::vector<size_t> ready;
    for (size_t i = 0; i < pool.m_aiSkillCastInterval.size(); ++i)
        if (pool.m_aiSkillCastInterval[i] <= Fixed{}) ready.push_back(i);
    std::sort(ready.begin(), ready.end(), [&](size_t a, size_t b) {
        const auto pa = firstValue(row.skillPriorityLevel, a, 1), pb = firstValue(row.skillPriorityLevel, b, 1);
        return pa == pb ? a < b : pa > pb;
    });
    for (const auto index : ready)
    {
        const std::string slot(1, static_cast<char>('A' + index));
        if (!skill::presetSkill(world, entity, slot, true)) continue;
        status::dealWithStatus(world, entity, EntityRoleStatus::Attack);
        const auto priority = firstValue(row.skillPriorityLevel, index, 1);
        auto& intervals = world.ecs().get<SkillPool>(entity).m_aiSkillCastInterval;
        for (size_t i = 0; i < row.skillPriorityLevel.size(); ++i)
            if (firstValue(row.skillPriorityLevel, i, 1) == priority)
            {
                if (intervals.size() <= i) intervals.resize(i + 1);
                intervals[i] = Fixed::fromInt(firstValue(row.skillPriorityLevelCd, i, row.skillInterval));
            }
        direction(world, entity, true);
        return Result::Running;
    }
    return Result::Success;
}
// 巡逻和游走先抽 X 再抽 Z；追击不消耗随机流。
Result move(World& world, Entity entity, Method method, const conf::AiConfig& row)
{
    const bool patrol = method == Method::ExecutePatrol, chase = method == Method::ExecuteChase;
    if (!chase && world.ecs().get<RoleStatus>(entity).isMove())
        return patrol ? Result::Running : Result::Success;
    if (!canAct(world, entity)) return Result::Failure;
    const auto self = world.ecs().get<Transform>(entity).position;
    FixedVec3 center = self;
    if (!patrol)
    {
        const auto* target = world.ecs().tryGet<Transform>(world.ecs().get<SkillPool>(entity).m_target);
        if (!target) return Result::Failure;
        center = target->position;
    }
    auto& rng = world.ecs().singleton<RngState>().random;
    const auto& xs = patrol ? row.patrolScopeX : row.chaseScopeX;
    const auto& zs = patrol ? row.patrolScopeZ : row.chaseScopeZ;
    const Fixed x = chase ? bound(xs, 1) : rng.randomF(bound(xs, 0), bound(xs, 1));
    const Fixed z = chase ? bound(zs, 1) : rng.randomF(bound(zs, 0), bound(zs, 1));
    const auto radius = world.ecs().get<ColliderState>(entity).radius;
    if (!chase && abs(x) < radius && abs(z) < radius) return Result::Failure;
    center.x += !patrol && center.x > self.x ? -x : x;
    center.z += !patrol && center.z > self.z ? -z : z;
    status::dealWithStatus(world, entity, patrol ? EntityRoleStatus::Patrol : chase ? EntityRoleStatus::Chase : EntityRoleStatus::Alert,
                          -1, calcPos(world, entity, center));
    return Result::Success;
}
// 瞬时上下文不进入组件快照。
struct Context { World& m_world; Entity m_entity; };
// 将无捕获函数指针转接到生产分派。
Result dispatch(void* value, Method method)
{
    auto& context = *static_cast<Context*>(value);
    return invoke(context.m_world, context.m_entity, method);
}
}

std::vector<Entity> candidates(World& world, Entity entity)
{
    std::vector<Entity> result;
    const int camp = world.ecs().get<RoleIdentity>(entity).camp;
    world.ecs().each<RoleIdentity, EntityGroup, Vitals>([&](Entity other, const RoleIdentity& identity, const EntityGroup& group, const Vitals&) {
        if (enabled(group) && camp != identity.camp && campGroup(camp) != 0 &&
            campGroup(camp) != campGroup(identity.camp) && hittable(world, other)) result.push_back(other);
    });
    return result;
}

Result checkTarget(World& world, Entity entity)
{
    if (world.ecs().get<RoleStatus>(entity).isDeath()) return Result::Failure;
    auto& target = world.ecs().get<SkillPool>(entity).m_target;
    const auto taunt = buff::sneerTarget(world, entity);
    if (alive(world, taunt)) { target = taunt; return Result::Success; }
    target = entity;
    const auto* row = config(world, entity);
    if (!row) return Result::Failure;
    const auto self = world.ecs().get<Transform>(entity).position;
    for (const auto other : candidates(world, entity))
    {
        const auto pos = world.ecs().get<Transform>(other).position;
        if (contains(row->targetScopeX, self.x - pos.x) && contains(row->targetScopeZ, self.z - pos.z))
        { target = other; return Result::Success; }
    }
    return Result::Failure;
}

Result invoke(World& world, Entity entity, Method method)
{
    const auto* row = config(world, entity);
    if (!row) return Result::Failure;
    const auto& state = world.ecs().get<RoleStatus>(entity);
    switch (method)
    {
    case Method::CheckAttack: return checkAttack(world, entity);
    case Method::CheckAttackEnd: return state.isAttack() ? Result::Running : Result::Success;
    case Method::CheckHit: return state.isTobeHit() ? Result::Success : Result::Failure;
    case Method::CheckHitEnd: return checkHitEnd(world, entity);
    case Method::CheckChase: return checkChase(world, entity, *row);
    case Method::CheckMoveEnd:
        if (state.isMove()) return Result::Running;
        return world.ecs().get<SkillPool>(entity).m_target == entity ? Result::Success : Result::Failure;
    case Method::CheckTarget: return checkTarget(world, entity);
    case Method::ExecuteRandAttack: return specialAttack(world, entity, false);
    case Method::ExecuteFirstAttack: return specialAttack(world, entity, true);
    case Method::ExecuteAttack: return executeAttack(world, entity, *row);
    case Method::ExecuteChase: case Method::ExecuteAlert: case Method::ExecutePatrol: return move(world, entity, method, *row);
    }
    MG_ASSERT(false && "Invalid AI method");
    return Result::Failure;
}

FixedVec3 calcPos(World& world, Entity entity, FixedVec3 point)
{
    const auto& map = world.ecs().singleton<MapRegion>();
    const auto radius = world.ecs().get<ColliderState>(entity).radius;
    auto edge = map.getLeftRegion({point.x - radius, point.z});
    if (point.x != edge) point.x = edge + radius;
    edge = map.getRightRegion({point.x + radius, point.z});
    if (point.x != edge) point.x = edge - radius;
    edge = map.getTopRegion({point.x, point.z + radius});
    if (point.z != edge) point.z = edge - radius;
    edge = map.getBottomRegion({point.x, point.z - radius});
    if (point.z != edge) point.z = edge + radius;
    if (world.ecs().get<RoleIdentity>(entity).roleType == 1)
    {
        const auto* role = conf::Config::instance().find<conf::RoleConfig>(world.ecs().get<RoleIdentity>(entity).roleId);
        if (role)
        {
            const Fixed distance = role->velocity * kLogicDtMs * conf::kWalkRate;
            const auto& self = world.ecs().get<Transform>(entity);
            if (abs(point.x - self.position.x) < distance) point.x += self.vectorX < Fixed{} ? -distance : distance;
            if (abs(point.z - self.position.z) < distance) point.z += self.vectorZ < Fixed{} ? -distance : distance;
        }
    }
    point.y = {};
    return point;
}

void addThreat(World& world, Entity entity, Entity source, Fixed damage)
{
    auto* agent = world.ecs().tryGet<AiAgent>(entity);
    if (!agent) return;
    for (auto& record : agent->m_threat)
        if (record.m_source == source) { record.m_damage = std::max(Fixed{}, record.m_damage + damage); return; }
    agent->m_threat.push_back({source, std::max(Fixed{}, damage)});
}

void changeTarget(World& world, Entity entity)
{
    auto& agent = world.ecs().get<AiAgent>(entity);
    auto& target = world.ecs().get<SkillPool>(entity).m_target;
    Fixed best;
    for (const auto& value : agent.m_threat) if (value.m_source == target) best = value.m_damage;
    for (const auto& value : agent.m_threat)
        if (value.m_damage > best && hittable(world, value.m_source)) { best = value.m_damage; target = value.m_source; }
    agent.m_threat.clear();
}

void update(World& world, Entity entity)
{
    auto& ecs = world.ecs();
    const auto* agent = ecs.tryGet<AiAgent>(entity);
    if (!agent || !agent->m_enabled || !ecs.get<RoleIdentity>(entity).isAi ||
        !enabled(ecs.get<EntityGroup>(entity)) || !ecs.get<ExtraState>(entity).isCanControl() || !config(world, entity)) return;
    Context context{world, entity};
    tickEntityTree(ecs.get<AiAgent>(entity).m_tree, &context, dispatch);
    const auto taunt = buff::sneerTarget(world, entity);
    if (alive(world, taunt)) { ecs.get<SkillPool>(entity).m_target = taunt; return; }
    auto& elapsed = ecs.get<AiAgent>(entity).m_changeTargetMs;
    elapsed += ecs.singleton<WorldClock>().dtMs;
    if (elapsed >= "10000"_fx) { elapsed -= "10000"_fx; changeTarget(world, entity); }
}
}
