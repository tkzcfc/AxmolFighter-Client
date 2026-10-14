#include "AiAgent.h"
#include "mugen/sim/World.h"
#include "mugen/sim/skill/SkillCaster.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/buff/BuffHolder.h"
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
// 二维配置的首项；缺失优先级或间隔按各自契约回退。
int32_t firstValue(const std::vector<std::vector<int32_t>>& values, size_t index, int32_t fallback)
{
    return index < values.size() && !values[index].empty() ? values[index][0] : fallback;
}
// 只读施法间隔：尚未写入池子时用配置里的初始冷却判断。
bool skillReady(const SkillPool& pool, const conf::AiConfig* row)
{
    if (!pool.m_aiSkillCastInterval.empty())
    {
        for (const Fixed value : pool.m_aiSkillCastInterval)
            if (value <= Fixed{}) return true;
        return false;
    }
    if (!row) return false;
    for (const auto& values : row->skillPriorityLevelCd)
        if ((values.empty() ? row->skillInterval : values.front()) <= 0) return true;
    return false;
}
// 巡逻、追击和游走对应条件表中的移动行。
int8_t ruleIndex(AiAction action)
{
    for (size_t index = 0; index < kMonsterRules.size(); ++index)
        if (kMonsterRules[index].m_action == action) return static_cast<int8_t>(index);
    return -1;
}
// 当前移动状态映射到正在跑的规则；没有移动时为 -1。
int8_t movingRule(const RoleStatus& state)
{
    if (state.isPatrol()) return ruleIndex(AiAction::Patrol);
    if (state.isChase()) return ruleIndex(AiAction::Chase);
    if (state.isAlert()) return ruleIndex(AiAction::Wander);
    return -1;
}
// 生命上限不为正时百分比为零。
int32_t hpPercent(const Vitals& vitals)
{
    if (vitals.maxHp <= Fixed{}) return 0;
    return static_cast<int32_t>((vitals.hp * Fixed::fromInt(100) / vitals.maxHp).toInt());
}
// 攻击状态视为已经开始的动作；其余非移动状态记为待机。
AiAction finishedAction(const RoleStatus& state)
{
    return state.isAttack() ? AiAction::CastBestSkill : AiAction::Idle;
}
}

AiDriver::AiDriver(World& world, Entity entity) : m_world(world), m_entity(entity) {}

// 在角色 AI 更新阶段执行决策、嘲讽和十秒仇恨切换。
void AiDriver::update()
{
    auto& ecs = m_world.ecs();
    const auto* agent = ecs.tryGet<AiAgent>(m_entity);
    if (!agent || !agent->m_enabled || !ecs.get<RoleIdentity>(m_entity).isAi ||
        !enabled(ecs.get<EntityGroup>(m_entity)) || !ecs.get<ExtraState>(m_entity).isCanControl() || !_config()) return;
    tickEntityTree(ecs.get<AiAgent>(m_entity).m_tree, this, dispatch);
    const auto taunt = buff::BuffHolder(m_world, m_entity).sneerTarget();
    if (_alive(taunt)) { ecs.get<SkillPool>(m_entity).m_target = taunt; return; }
    auto& elapsed = ecs.get<AiAgent>(m_entity).m_changeTargetMs;
    elapsed += ecs.singleton<WorldClock>().dtMs;
    if (elapsed >= "10000"_fx) { elapsed -= "10000"_fx; changeTarget(); }
}

// 分派生产怪物方法，供决策树和动作叶子共用。
Result AiDriver::invoke(Method method)
{
    const auto* row = _config();
    if (!row) return Result::Failure;
    const auto& state = m_world.ecs().get<RoleStatus>(m_entity);
    switch (method)
    {
    case Method::CheckAttack: return _checkAttack();
    case Method::CheckAttackEnd: return state.isAttack() ? Result::Running : Result::Success;
    case Method::CheckHit: return state.isTobeHit() ? Result::Success : Result::Failure;
    case Method::CheckHitEnd: return _checkHitEnd();
    case Method::CheckChase: return _checkChase(*row);
    case Method::CheckMoveEnd:
        if (state.isMove()) return Result::Running;
        return m_world.ecs().get<SkillPool>(m_entity).m_target == m_entity ? Result::Success : Result::Failure;
    case Method::CheckTarget: return checkTarget();
    case Method::ExecuteRandAttack: return _specialAttack(false);
    case Method::ExecuteFirstAttack: return _specialAttack(true);
    case Method::ExecuteAttack: return _executeAttack(*row);
    case Method::ExecuteChase: case Method::ExecuteAlert: case Method::ExecutePatrol: return _move(method, *row);
    }
    MG_ASSERT(false && "Invalid AI method");
    return Result::Failure;
}

// 每帧巡逻索敌；使用带符号的视野区间。
Result AiDriver::checkTarget()
{
    if (m_world.ecs().get<RoleStatus>(m_entity).isDeath()) return Result::Failure;
    auto& target = m_world.ecs().get<SkillPool>(m_entity).m_target;
    const auto taunt = buff::BuffHolder(m_world, m_entity).sneerTarget();
    if (_alive(taunt)) { target = taunt; return Result::Success; }
    target = m_entity;
    const auto* row = _config();
    if (!row) return Result::Failure;
    const auto self = m_world.ecs().get<Transform>(m_entity).position;
    for (const auto other : candidates())
    {
        const auto pos = m_world.ecs().get<Transform>(other).position;
        if (contains(row->targetScopeX, self.x - pos.x) && contains(row->targetScopeZ, self.z - pos.z))
        { target = other; return Result::Success; }
    }
    return Result::Failure;
}

// 记录命中伤害，不为非 AI 角色创建状态。
void AiDriver::addThreat(Entity source, Fixed damage)
{
    auto* agent = m_world.ecs().tryGet<AiAgent>(m_entity);
    if (!agent) return;
    for (auto& record : agent->m_threat)
        if (record.m_source == source) { record.m_damage = std::max(Fixed{}, record.m_damage + damage); return; }
    agent->m_threat.push_back({source, std::max(Fixed{}, damage)});
}

// 选择严格高于当前目标权重的有效攻击者，然后清空本周期记录。
void AiDriver::changeTarget()
{
    auto& agent = m_world.ecs().get<AiAgent>(m_entity);
    auto& target = m_world.ecs().get<SkillPool>(m_entity).m_target;
    Fixed best;
    for (const auto& value : agent.m_threat) if (value.m_source == target) best = value.m_damage;
    for (const auto& value : agent.m_threat)
        if (value.m_damage > best && _hittable(value.m_source)) { best = value.m_damage; target = value.m_source; }
    agent.m_threat.clear();
}

// 按地图四侧边界和角色半径修正目标点。
FixedVec3 AiDriver::calcPos(FixedVec3 point) const
{
    const auto& map = m_world.ecs().singleton<MapRegion>();
    const auto radius = m_world.ecs().get<ColliderState>(m_entity).radius;
    auto edge = map.getLeftRegion({point.x - radius, point.z});
    if (point.x != edge) point.x = edge + radius;
    edge = map.getRightRegion({point.x + radius, point.z});
    if (point.x != edge) point.x = edge - radius;
    edge = map.getTopRegion({point.x, point.z + radius});
    if (point.z != edge) point.z = edge - radius;
    edge = map.getBottomRegion({point.x, point.z - radius});
    if (point.z != edge) point.z = edge + radius;
    if (m_world.ecs().get<RoleIdentity>(m_entity).roleType == 1)
    {
        const auto* role = conf::Config::instance().find<conf::RoleConfig>(m_world.ecs().get<RoleIdentity>(m_entity).roleId);
        if (role)
        {
            const Fixed distance = role->velocity * kLogicDtMs * conf::kWalkRate;
            const auto& self = m_world.ecs().get<Transform>(m_entity);
            if (abs(point.x - self.position.x) < distance) point.x += self.vectorX < Fixed{} ? -distance : distance;
            if (abs(point.z - self.position.z) < distance) point.z += self.vectorZ < Fixed{} ? -distance : distance;
        }
    }
    point.y = {};
    return point;
}

// 以固定创建顺序收集存活、启用且可命中的敌对阵营组角色。
std::vector<Entity> AiDriver::candidates() const
{
    std::vector<Entity> result;
    const int camp = m_world.ecs().get<RoleIdentity>(m_entity).camp;
    m_world.ecs().each<RoleIdentity, EntityGroup, Vitals>([&](Entity other, const RoleIdentity& identity, const EntityGroup& group, const Vitals&) {
        if (enabled(group) && camp != identity.camp && campGroup(camp) != 0 &&
            campGroup(camp) != campGroup(identity.camp) && _hittable(other)) result.push_back(other);
    });
    return result;
}

// 从当前组件和 AI 配置生成决策快照；不改状态，也不消耗随机数。
AiFacts AiDriver::gather() const
{
    auto& ecs          = m_world.ecs();
    const auto& state  = ecs.get<RoleStatus>(m_entity);
    const auto& vitals = ecs.get<Vitals>(m_entity);
    const auto& pool   = ecs.get<SkillPool>(m_entity);
    const auto* row    = conf::Config::instance().find<conf::AiConfig>(ecs.get<RoleIdentity>(m_entity).aiId);
    AiFacts facts;
    facts.m_alerted            = ecs.get<AiAgent>(m_entity).m_tree.m_alert == AlertStatus::Alert;
    facts.m_provoked           = _provoked();
    facts.m_hasTarget          = pool.m_target != m_entity && ecs.tryGet<Transform>(pool.m_target) != nullptr;
    facts.m_targetInSight      = row && _inSight(*row);
    facts.m_targetOutsideChase = row && facts.m_hasTarget && _outsideChase(pool.m_target, *row);
    facts.m_skillReady         = skillReady(pool, row);
    facts.m_movingRule         = movingRule(state);
    facts.m_lastAction         = facts.m_movingRule >= 0 ? AiAction::Idle : finishedAction(state);
    facts.m_hpPercent          = hpPercent(vitals);
    facts.m_inAttack           = state.isAttack();
    return facts;
}

// 将无捕获函数指针转接到生产分派。
Result AiDriver::dispatch(void* value, Method method)
{
    return static_cast<AiDriver*>(value)->invoke(method);
}

// 存活句柄和死亡状态共同限制跨实体访问。
bool AiDriver::_alive(Entity entity) const
{
    const auto* state = m_world.ecs().tryGet<RoleStatus>(entity);
    return state && !state->isDeath() && !Role(m_world, entity).isDead();
}

// 不可命中状态不参与普通 AI 目标选择。
bool AiDriver::_hittable(Entity entity) const
{
    return _alive(entity) && m_world.ecs().get<Vitals>(entity).m_beHitEnabled &&
           !m_world.ecs().get<ExtraState>(entity).active(mugen::ExtraState::Stealth);
}

// 获取当前角色配置，悬空引用由入口拒绝。
const conf::AiConfig* AiDriver::_config() const
{
    return conf::Config::instance().find<conf::AiConfig>(m_world.ecs().get<RoleIdentity>(m_entity).aiId);
}

// 死亡、受击和失控时不能执行主动决策动作。
bool AiDriver::_canAct() const
{
    const auto& state = m_world.ecs().get<RoleStatus>(m_entity);
    return !state.isDeath() && !state.isTobeHit() &&
           m_world.ecs().get<ExtraState>(m_entity).isCanControl();
}

// 先设置技能控制角，成功释放后再更新角色朝向。
void AiDriver::_direction(bool apply)
{
    auto& ecs = m_world.ecs();
    const auto target = ecs.get<SkillPool>(m_entity).m_target;
    const auto* other = ecs.tryGet<Transform>(target);
    if (!other) return;
    auto& self = ecs.get<Transform>(m_entity);
    const Fixed x = other->position.x - self.position.x, z = other->position.z - self.position.z;
    if (x == Fixed{} && z == Fixed{}) return;
    auto angle = "90"_fx - FixedMath::atan2Deg(z, x);
    if (angle < Fixed{}) angle += "360"_fx;
    ecs.get<SkillPool>(m_entity).m_controlAngle = angle;
    if (apply)
    {
        if (x != Fixed{}) { self.towardX = x > Fixed{} ? 1 : -1; self.vectorX = Fixed::fromInt(self.towardX); }
        if (z != Fixed{}) self.vectorZ = z > Fixed{} ? "1"_fx : "-1"_fx;
    }
}

// 目标失效时从稳定候选数组随机重选；无目标使用自身哨兵。
Result AiDriver::_checkAttack()
{
    if (m_world.ecs().get<RoleStatus>(m_entity).isDeath()) return Result::Failure;
    auto& target = m_world.ecs().get<SkillPool>(m_entity).m_target;
    const auto taunt = buff::BuffHolder(m_world, m_entity).sneerTarget();
    if (_alive(taunt)) { target = taunt; return Result::Success; }
    if (!_hittable(target))
    {
        const auto enemies = candidates();
        if (enemies.empty()) { target = m_entity; return Result::Failure; }
        const auto index = m_world.ecs().singleton<RngState>().random.randomN(0, static_cast<int32_t>(enemies.size() - 1));
        target = enemies[index];
    }
    return Result::Success;
}

// 受击结束只在目标句柄缺失时补选，不重置警戒。
Result AiDriver::_checkHitEnd()
{
    if (m_world.ecs().get<RoleStatus>(m_entity).isTobeHit()) return Result::Running;
    auto& target = m_world.ecs().get<SkillPool>(m_entity).m_target;
    if (!m_world.ecs().tryGet<RoleIdentity>(target))
    {
        const auto enemies = candidates();
        if (enemies.empty()) return Result::Failure;
        target = enemies[m_world.ecs().singleton<RngState>().random.randomN(0, static_cast<int32_t>(enemies.size() - 1))];
    }
    return Result::Success;
}

// 原始追击条件保留坐标轴为零时的特殊分支。
Result AiDriver::_checkChase(const conf::AiConfig& row)
{
    const auto* target = m_world.ecs().tryGet<Transform>(m_world.ecs().get<SkillPool>(m_entity).m_target);
    if (!target || !_canAct()) return Result::Failure;
    if (bound(row.chaseScopeX, 0) == Fixed{} && bound(row.chaseScopeX, 1) == Fixed{} &&
        bound(row.chaseScopeZ, 0) == Fixed{} && bound(row.chaseScopeZ, 1) == Fixed{}) return Result::Failure;
    const auto self = m_world.ecs().get<Transform>(m_entity).position;
    const Fixed x = self.x - target->position.x, z = self.z - target->position.z;
    if (x == Fixed{} && z == Fixed{}) return Result::Failure;
    if (x != Fixed{} && z != Fixed{} && contains(row.chaseScopeX, abs(x)) && contains(row.chaseScopeZ, abs(z)))
        return Result::Failure;
    return Result::Success;
}

// X/Z 使用手动施放门槛，已有技能时续接原槽。
Result AiDriver::_specialAttack(bool first)
{
    if (!_canAct()) return Result::Failure;
    if (!first && candidates().empty())
    {
        m_world.ecs().get<SkillPool>(m_entity).m_target = m_entity;
        return Result::Running;
    }
    if (first) _direction(false);
    const auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    const std::string slot = pool.m_current == kNoSkill ? (first ? "X" : "Z") :
        pool.m_slots[pool.m_skills[pool.m_current].m_address.m_slot].m_name;
    if (!skill::SkillCaster(m_world, m_entity).presetSkill(slot, false)) return Result::Success;
    Role(m_world, m_entity).changeStatus(EntityRoleStatus::Attack);
    if (first) _direction(true);
    return Result::Running;
}

// 普通槽按优先级降序、原索引升序检查，成功后重置同优先级间隔。
Result AiDriver::_executeAttack(const conf::AiConfig& row)
{
    if (candidates().empty() || !_canAct() ||
        !m_world.ecs().tryGet<Transform>(m_world.ecs().get<SkillPool>(m_entity).m_target)) return Result::Failure;
    _direction(false);
    auto& pool = m_world.ecs().get<SkillPool>(m_entity);
    if (pool.m_current != kNoSkill)
    {
        const auto slot = pool.m_slots[pool.m_skills[pool.m_current].m_address.m_slot].m_name;
        if (!skill::SkillCaster(m_world, m_entity).presetSkill(slot, true)) return Result::Success;
        Role(m_world, m_entity).changeStatus(EntityRoleStatus::Attack);
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
        if (!skill::SkillCaster(m_world, m_entity).presetSkill(slot, true)) continue;
        Role(m_world, m_entity).changeStatus(EntityRoleStatus::Attack);
        const auto priority = firstValue(row.skillPriorityLevel, index, 1);
        auto& intervals = m_world.ecs().get<SkillPool>(m_entity).m_aiSkillCastInterval;
        for (size_t i = 0; i < row.skillPriorityLevel.size(); ++i)
            if (firstValue(row.skillPriorityLevel, i, 1) == priority)
            {
                if (intervals.size() <= i) intervals.resize(i + 1);
                intervals[i] = Fixed::fromInt(firstValue(row.skillPriorityLevelCd, i, row.skillInterval));
            }
        _direction(true);
        return Result::Running;
    }
    return Result::Success;
}

// 巡逻和游走先抽 X 再抽 Z；追击不消耗随机流。
Result AiDriver::_move(Method method, const conf::AiConfig& row)
{
    const bool patrol = method == Method::ExecutePatrol, chase = method == Method::ExecuteChase;
    if (!chase && m_world.ecs().get<RoleStatus>(m_entity).isMove())
        return patrol ? Result::Running : Result::Success;
    if (!_canAct()) return Result::Failure;
    const auto self = m_world.ecs().get<Transform>(m_entity).position;
    FixedVec3 center = self;
    if (!patrol)
    {
        const auto* target = m_world.ecs().tryGet<Transform>(m_world.ecs().get<SkillPool>(m_entity).m_target);
        if (!target) return Result::Failure;
        center = target->position;
    }
    auto& rng = m_world.ecs().singleton<RngState>().random;
    const auto& xs = patrol ? row.patrolScopeX : row.chaseScopeX;
    const auto& zs = patrol ? row.patrolScopeZ : row.chaseScopeZ;
    const Fixed x = chase ? bound(xs, 1) : rng.randomF(bound(xs, 0), bound(xs, 1));
    const Fixed z = chase ? bound(zs, 1) : rng.randomF(bound(zs, 0), bound(zs, 1));
    const auto radius = m_world.ecs().get<ColliderState>(m_entity).radius;
    if (!chase && abs(x) < radius && abs(z) < radius) return Result::Failure;
    center.x += !patrol && center.x > self.x ? -x : x;
    center.z += !patrol && center.z > self.z ? -z : z;
    Role(m_world, m_entity).changeStatus(patrol ? EntityRoleStatus::Patrol : chase ? EntityRoleStatus::Chase : EntityRoleStatus::Alert,
                                        -1, calcPos(center));
    return Result::Success;
}

// 受击、仇恨记录或有效嘲讽都算激怒。
bool AiDriver::_provoked() const
{
    if (m_world.ecs().get<RoleStatus>(m_entity).isTobeHit()) return true;
    if (!m_world.ecs().get<AiAgent>(m_entity).m_threat.empty()) return true;
    return _alive(buff::BuffHolder(m_world, m_entity).sneerTarget());
}

// 索敌使用带符号的视野区间，先看已锁定目标再看候选者。
bool AiDriver::_inSight(const conf::AiConfig& row) const
{
    const auto origin = m_world.ecs().get<Transform>(m_entity).position;
    const auto locked = m_world.ecs().get<SkillPool>(m_entity).m_target;
    if (const auto* other = m_world.ecs().tryGet<Transform>(locked))
        if (locked != m_entity && contains(row.targetScopeX, origin.x - other->position.x) &&
            contains(row.targetScopeZ, origin.z - other->position.z))
            return true;
    for (const auto other : candidates())
    {
        const auto pos = m_world.ecs().get<Transform>(other).position;
        if (contains(row.targetScopeX, origin.x - pos.x) && contains(row.targetScopeZ, origin.z - pos.z)) return true;
    }
    return false;
}

// 追击范围全零、重合或落在范围内时都不算范围外。
bool AiDriver::_outsideChase(Entity target, const conf::AiConfig& row) const
{
    const auto* other = m_world.ecs().tryGet<Transform>(target);
    if (!other) return false;
    if (bound(row.chaseScopeX, 0) == Fixed{} && bound(row.chaseScopeX, 1) == Fixed{} &&
        bound(row.chaseScopeZ, 0) == Fixed{} && bound(row.chaseScopeZ, 1) == Fixed{})
        return false;
    const auto self = m_world.ecs().get<Transform>(m_entity).position;
    const Fixed x = self.x - other->position.x, z = self.z - other->position.z;
    if (x == Fixed{} && z == Fixed{}) return false;
    if (x != Fixed{} && z != Fixed{} && contains(row.chaseScopeX, abs(x)) && contains(row.chaseScopeZ, abs(z)))
        return false;
    return true;
}
}  // namespace mugen::sim::ai
