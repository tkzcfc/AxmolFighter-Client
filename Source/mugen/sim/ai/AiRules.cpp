#include "mugen/sim/ai/AiRules.h"
#include "mugen/sim/ai/AiComponents.h"
#include "mugen/sim/World.h"
#include "mugen/sim/base/Rng.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/battle/AiDef.h"
#include "mugen/conf/battle/RoleDef.h"
#include "mugen/conf/battle/SkillDef.h"
#include "mugen/core/math/FixedMath.h"
#include "mugen/core/math/FixedVec2.h"
#include "mugen/core/MacroDefinition.h"
#include <algorithm>
#include <string>
#include <vector>

namespace mugen::sim::ai
{
namespace
{
// 定点绝对值不经过宿主浮点重载。
Fixed absValue(Fixed value)
{
    return value < Fixed{} ? -value : value;
}
// 阵营组而非攻击掩码决定 AI 的候选范围。
int campGroup(int camp)
{
    switch (camp)
    {
    case 1:
        return 1;
    case 2:
    case 4:
        return 2;
    case 8:
    case 16:
        return 3;
    default:
        return 0;
    }
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
// 只读施法间隔：尚未写入时用配置里的初始冷却判断。
bool skillReady(const std::vector<Fixed>& intervals, const conf::battle::AiDef* row)
{
    if (!intervals.empty())
    {
        for (const Fixed value : intervals)
            if (value <= Fixed{})
                return true;
        return false;
    }
    if (!row)
        return false;
    for (const auto& values : row->m_skillPriorityLevelCd)
        if ((values.empty() ? row->m_skillInterval : values.front()) <= 0)
            return true;
    return false;
}
// 巡逻、追击和游走对应条件表中的移动行。
int8_t ruleIndex(AiAction action)
{
    for (size_t index = 0; index < kMonsterRules.size(); ++index)
        if (kMonsterRules[index].m_action == action)
            return static_cast<int8_t>(index);
    return -1;
}
// 动作是否属于移动类（追击 / 巡逻 / 游走）。
bool isMoveAction(AiAction action)
{
    return action == AiAction::Chase || action == AiAction::Patrol || action == AiAction::Wander;
}
// 生命上限不为正时百分比为零。
int32_t hpPercent(const HealthComponent& health)
{
    if (health.m_maxHp <= Fixed{})
        return 0;
    return static_cast<int32_t>((health.m_hp * Fixed::fromInt(100) / health.m_maxHp).toInt());
}
// 按角色配置与 1-based 难度下标解析 aiId；难度越界回退最后一项（与旧实现 RoleSpawner 一致）。
int32_t resolveAiId(const conf::battle::RoleDef& role, int32_t aiDifficulty)
{
    if (role.aiId.empty())
        return 0;
    const size_t difficulty = aiDifficulty > 0 && static_cast<size_t>(aiDifficulty) <= role.aiId.size()
                                  ? static_cast<size_t>(aiDifficulty - 1)
                                  : role.aiId.size() - 1;
    return role.aiId[difficulty];
}
// 目标是否存活：非死亡状态且生命未清空。
bool alive(World& world, Entity entity)
{
    if (!world.ecs().has<RoleStateComponent>(entity))
        return false;
    if (world.ecs().get<RoleStateComponent>(entity).m_state == RoleStateId::Death)
        return false;
    return !(world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).m_dead);
}
// 不可命中状态不参与普通 AI 目标选择。
bool hittable(World& world, Entity entity)
{
    if (!alive(world, entity))
        return false;
    if (world.ecs().has<HealthComponent>(entity) && !world.ecs().get<HealthComponent>(entity).m_beHitEnabled)
        return false;
    return !(world.ecs().has<StatusFlagsComponent>(entity) &&
             world.ecs().get<StatusFlagsComponent>(entity).active(mugen::ExtraState::Stealth));
}
// 数值最高且有效的嘲讽施加者；没有则返回空句柄。
Entity sneerTarget(World& world, Entity self)
{
    if (!world.ecs().has<TauntTableComponent>(self))
        return {};
    Entity best;
    Fixed bestWeight;
    for (const auto& entry : world.ecs().get<TauntTableComponent>(self).m_entries)
        if (entry.m_weight > bestWeight)
        {
            bestWeight = entry.m_weight;
            best       = entry.m_applicator;
        }
    return best;
}
// 受击、仇恨记录或有效嘲讽都算激怒。
bool provoked(World& world, Entity self, const AiBrainComponent& brain)
{
    if (role::isTobeHit(world.ecs().get<RoleStateComponent>(self).m_state))
        return true;
    if (!brain.m_threat.empty())
        return true;
    return alive(world, sneerTarget(world, self));
}
// 死亡、受击和失控时不能执行主动决策动作。
bool canAct(World& world, Entity self)
{
    const auto& state = world.ecs().get<RoleStateComponent>(self);
    if (state.m_state == RoleStateId::Death || role::isTobeHit(state.m_state))
        return false;
    return !world.ecs().has<StatusFlagsComponent>(self) ||
           world.ecs().get<StatusFlagsComponent>(self).canControl();
}
// 以固定创建顺序收集存活、启用且可命中的敌对阵营组角色。
std::vector<Entity> candidates(World& world, Entity self)
{
    std::vector<Entity> result;
    const int camp = world.ecs().get<IdentityComponent>(self).m_camp;
    world.ecs().each<IdentityComponent, HealthComponent>(
        [&](Entity other, const IdentityComponent& identity, const HealthComponent&) {
        if (world.ecs().has<InactiveComponent>(other) || world.ecs().has<DespawningComponent>(other))
            return;
        if (identity.m_kind != SpawnKind::Role)
            return;
        if (camp == identity.m_camp || campGroup(camp) == 0 || campGroup(camp) == campGroup(identity.m_camp))
            return;
        if (hittable(world, other))
            result.push_back(other);
    });
    return result;
}
// 先看已锁定目标再按创建顺序看候选者，用带符号的索敌区间；没有目标时返回自身哨兵。
Entity sightTarget(World& world, Entity self, const AiBrainComponent& brain, const conf::battle::AiDef& row)
{
    const FixedVec3 origin = world.ecs().get<TransformComponent>(self).m_position;
    const Entity locked    = brain.m_target;
    if (const auto* other = world.ecs().tryGet<TransformComponent>(locked))
        if (locked != self && contains(row.m_targetScopeX, origin.x - other->m_position.x) &&
            contains(row.m_targetScopeZ, origin.z - other->m_position.z))
            return locked;
    for (const Entity other : candidates(world, self))
    {
        const auto pos = world.ecs().get<TransformComponent>(other).m_position;
        if (contains(row.m_targetScopeX, origin.x - pos.x) && contains(row.m_targetScopeZ, origin.z - pos.z))
            return other;
    }
    return self;
}
// 追击范围全零、重合或落在范围内时都不算范围外。
bool outsideChase(World& world, Entity self, Entity target, const conf::battle::AiDef& row)
{
    const auto* other = world.ecs().tryGet<TransformComponent>(target);
    if (!other)
        return false;
    if (bound(row.m_chaseScopeX, 0) == Fixed{} && bound(row.m_chaseScopeX, 1) == Fixed{} &&
        bound(row.m_chaseScopeZ, 0) == Fixed{} && bound(row.m_chaseScopeZ, 1) == Fixed{})
        return false;
    const auto selfPos = world.ecs().get<TransformComponent>(self).m_position;
    const Fixed x      = selfPos.x - other->m_position.x;
    const Fixed z      = selfPos.z - other->m_position.z;
    if (x == Fixed{} && z == Fixed{})
        return false;
    if (x != Fixed{} && z != Fixed{} && contains(row.m_chaseScopeX, absValue(x)) &&
        contains(row.m_chaseScopeZ, absValue(z)))
        return false;
    return true;
}
// 按地图四侧边界和角色半径修正目标点。
FixedVec3 calcPos(World& world, Entity self, FixedVec3 point)
{
    const auto& map    = world.ecs().singleton<MapRegion>();
    const Fixed radius = world.ecs().has<HitboxesComponent>(self) ? world.ecs().get<HitboxesComponent>(self).m_radius
                                                                  : Fixed{};
    auto edge = map.getLeftRegion({point.x - radius, point.z});
    if (point.x != edge)
        point.x = edge + radius;
    edge = map.getRightRegion({point.x + radius, point.z});
    if (point.x != edge)
        point.x = edge - radius;
    edge = map.getTopRegion({point.x, point.z + radius});
    if (point.z != edge)
        point.z = edge - radius;
    edge = map.getBottomRegion({point.x, point.z - radius});
    if (point.z != edge)
        point.z = edge + radius;
    point.y = {};
    return point;
}
// 从当前组件和 AI 配置生成决策快照；不改状态，也不消耗随机数。
AiFacts gather(World& world, Entity self, const AiBrainComponent& brain, const conf::battle::AiDef& row)
{
    AiFacts facts;
    const auto& state  = world.ecs().get<RoleStateComponent>(self);
    const auto& health = world.ecs().get<HealthComponent>(self);
    const int32_t current =
        world.ecs().has<SkillCastComponent>(self) ? world.ecs().get<SkillCastComponent>(self).m_skill : kNoSkill;
    facts.m_alerted            = brain.m_alerted;
    facts.m_provoked           = provoked(world, self, brain);
    facts.m_hasTarget          = brain.m_target != self && world.ecs().tryGet<TransformComponent>(brain.m_target) != nullptr;
    facts.m_targetInSight      = sightTarget(world, self, brain, row) != self;
    facts.m_targetOutsideChase = facts.m_hasTarget && outsideChase(world, self, brain.m_target, row);
    facts.m_skillReady         = current != kNoSkill || skillReady(brain.m_skillIntervalMs, &row);
    // 只有角色仍在移动状态时，移动规则才继续占据决策；施法/待机时恢复完整规则匹配。
    facts.m_movingRule         = role::isMoveState(state.m_state) ? brain.m_movingRule : -1;
    facts.m_lastAction         = facts.m_movingRule >= 0 ? AiAction::Idle : brain.m_lastAction;
    facts.m_hpPercent          = hpPercent(health);
    facts.m_inAttack           = state.m_state == RoleStateId::Attack;
    return facts;
}
// 写入一条朝向 targetPos 的移动意图，怪物沿既有位移推进（与玩家共用路径）。
// 怪物移动动作用行走态（"running" 动作，kWalkRate / kWalkScaleTime），与旧实现 AiMove 一致；
// 跑动态（"walk" 动作）仅玩家普通移动使用，怪物 motion 通常不含该动作。
void writeMoveIntent(World& world, Entity self, FixedVec3 to)
{
    auto& intent        = world.ecs().get<IntentComponent>(self);
    const FixedVec3 from = world.ecs().get<TransformComponent>(self).m_position;
    const Fixed dx = to.x - from.x, dz = to.z - from.z;
    if (dx == Fixed{} && dz == Fixed{})
    {
        intent.m_move.m_mode     = MoveMode::Stop;
        intent.m_move.m_quadrant = ControlQuadrant::None;
        intent.m_move.m_angle    = 0;
        intent.m_move.m_targetPos = to;
        return;
    }
    intent.m_move.m_mode = MoveMode::Walk;
    intent.m_move.m_targetPos = to;
    // 方向由象限承载，斜率角只给 0–90°（0 轴对齐、90000 纯垂直）。
    intent.m_move.m_quadrant = dx >= Fixed{} ? (dz >= Fixed{} ? ControlQuadrant::UpRight : ControlQuadrant::DownRight)
                                             : (dz >= Fixed{} ? ControlQuadrant::UpLeft : ControlQuadrant::DownLeft);
    const Fixed deg = FixedMath::atan2Deg(absValue(dz), absValue(dx));
    intent.m_move.m_angle = (deg * Fixed::fromInt(1000)).toInt();
}
// 写入一条施法按键意图（按下类型 1），并标记为自动施法。
void writeCastIntent(World& world, Entity self, ControlEvent event)
{
    auto& intent = world.ecs().get<IntentComponent>(self);
    intent.m_buttons.push_back({event, 1, 0});
    intent.m_autoCast = true;
}
// 通过按键通道请求醒/待机技能（otherSkillIds 分别落槽 H/I/J）。first 为醒技能，否则待机技能。
bool castSpecial(World& world, Entity self, bool first, const conf::battle::AiDef& row)
{
    if (!canAct(world, self))
        return false;
    if (!first && candidates(world, self).empty())
        return false;
    const size_t otherIndex = first ? 0 : 2;
    if (otherIndex >= row.m_otherSkillIds.size())
        return false;
    if (firstValue(row.m_otherSkillIds, otherIndex, -1) == -1)
        return false;
    const auto event = static_cast<ControlEvent>(static_cast<int32_t>(ControlEvent::CTR_ATKH) + otherIndex);
    writeCastIntent(world, self, event);
    return true;
}
// 普通槽按优先级降序、原索引升序检查，成功后重置同优先级间隔。
bool castBest(World& world, Entity self, AiBrainComponent& brain, const conf::battle::AiDef& row)
{
    if (candidates(world, self).empty() || !canAct(world, self))
        return false;
    auto& intervals = brain.m_skillIntervalMs;
    if (intervals.empty())
        for (const auto& values : row.m_skillPriorityLevelCd)
            intervals.push_back(Fixed::fromInt(values.empty() ? row.m_skillInterval : values[0]));
    std::vector<size_t> ready;
    for (size_t i = 0; i < intervals.size(); ++i)
        if (intervals[i] <= Fixed{})
            ready.push_back(i);
    std::sort(ready.begin(), ready.end(), [&](size_t a, size_t b) {
        const auto pa = firstValue(row.m_skillPriorityLevel, a, 1), pb = firstValue(row.m_skillPriorityLevel, b, 1);
        return pa == pb ? a < b : pa > pb;
    });
    for (const size_t index : ready)
    {
        if (index >= row.m_skillIds.size() || firstValue(row.m_skillIds, index, -1) == -1)
            continue;
        const auto event = static_cast<ControlEvent>(static_cast<int32_t>(ControlEvent::CTR_ATKA) + index);
        writeCastIntent(world, self, event);
        const auto priority = firstValue(row.m_skillPriorityLevel, index, 1);
        for (size_t i = 0; i < row.m_skillPriorityLevel.size(); ++i)
            if (firstValue(row.m_skillPriorityLevel, i, 1) == priority)
            {
                if (intervals.size() <= i)
                    intervals.resize(i + 1);
                intervals[i] = Fixed::fromInt(firstValue(row.m_skillPriorityLevelCd, i, row.m_skillInterval));
            }
        return true;
    }
    return false;
}
// 巡逻和游走先抽 X 再抽 Z；追击不消耗随机流。
bool move(World& world, Entity self, AiBrainComponent& brain, AiAction action, const conf::battle::AiDef& row)
{
    const bool patrol = action == AiAction::Patrol, chase = action == AiAction::Chase;
    if (!chase && role::isMoveState(world.ecs().get<RoleStateComponent>(self).m_state))
        return patrol;
    if (!canAct(world, self))
        return false;
    const FixedVec3 selfPos = world.ecs().get<TransformComponent>(self).m_position;
    FixedVec3 center        = selfPos;
    if (!patrol)
    {
        const auto* target = world.ecs().tryGet<TransformComponent>(brain.m_target);
        if (!target)
            return false;
        center = target->m_position;
    }
    const auto& xs    = patrol ? row.m_patrolScopeX : row.m_chaseScopeX;
    const auto& zs    = patrol ? row.m_patrolScopeZ : row.m_chaseScopeZ;
    const Fixed x     = chase ? bound(xs, 1) : rng::drawF(world, rng::DrawKind::AiWander, bound(xs, 0), bound(xs, 1));
    const Fixed z     = chase ? bound(zs, 1) : rng::drawF(world, rng::DrawKind::AiWander, bound(zs, 0), bound(zs, 1));
    const Fixed radius = world.ecs().has<HitboxesComponent>(self) ? world.ecs().get<HitboxesComponent>(self).m_radius
                                                                  : Fixed{};
    if (!chase && absValue(x) < radius && absValue(z) < radius)
        return false;
    center.x += !patrol && center.x > selfPos.x ? -x : x;
    center.z += !patrol && center.z > selfPos.z ? -z : z;
    const FixedVec3 goal = calcPos(world, self, center);
    brain.m_moveTarget   = goal;
    writeMoveIntent(world, self, goal);
    brain.m_movingRule = ruleIndex(action);
    return true;
}
// 延续既有移动：朝 m_moveTarget 续写移动意图；到达（近于一帧步长）则停下并返回 false。
bool continueMove(World& world, Entity self, AiBrainComponent& brain)
{
    auto& ecs            = world.ecs();
    const FixedVec3 pos  = ecs.get<TransformComponent>(self).m_position;
    const FixedVec3 goal = brain.m_moveTarget;
    const Fixed dx       = goal.x - pos.x, dz = goal.z - pos.z;
    Fixed step           = ecs.singleton<WorldClock>().m_dtMs;
    const auto& identity = ecs.get<IdentityComponent>(self);
    if (const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId))
        step = role->velocity * conf::kWalkRate * ecs.singleton<WorldClock>().m_dtMs;
    if (absValue(dx) <= step && absValue(dz) <= step)
    {
        auto& intent        = ecs.get<IntentComponent>(self);
        intent.m_move.m_mode     = MoveMode::Stop;
        intent.m_move.m_quadrant = ControlQuadrant::None;
        intent.m_move.m_angle    = 0;
        return false;
    }
    writeMoveIntent(world, self, goal);
    return true;
}
// 执行命中规则的动作，返回动作是否真的启动。
bool run(World& world, Entity self, AiBrainComponent& brain, AiAction action, const conf::battle::AiDef& row)
{
    switch (action)
    {
    case AiAction::Idle:
        return false;
    case AiAction::CastWakeSkill:
        return castSpecial(world, self, true, row);
    case AiAction::CastIdleSkill:
        return castSpecial(world, self, false, row);
    case AiAction::CastBestSkill:
        return castBest(world, self, brain, row);
    case AiAction::Patrol:
    case AiAction::Chase:
    case AiAction::Wander:
        return move(world, self, brain, action, row);
    }
    MG_ASSERT(false && "Invalid AI action");
    return false;
}
// 按警戒、激怒和候选者选择索敌目标。
void selectTarget(World& world, Entity self, AiBrainComponent& brain, const conf::battle::AiDef& row)
{
    const Entity taunt = sneerTarget(world, self);
    if (alive(world, taunt))
    {
        brain.m_target = taunt;
        return;
    }
    if (!brain.m_alerted && !provoked(world, self, brain))
    {
        brain.m_target = sightTarget(world, self, brain, row);
        return;
    }
    if (hittable(world, brain.m_target))
        return;
    const auto enemies = candidates(world, self);
    if (enemies.empty())
    {
        brain.m_target = self;
        return;
    }
    brain.m_target = enemies[static_cast<size_t>(rng::drawN(world, rng::DrawKind::AiTarget, 0,
                                                            static_cast<int32_t>(enemies.size() - 1)))];
}
// 选择严格高于当前目标权重的有效攻击者，然后清空本周期记录。
void changeTarget(World& world, AiBrainComponent& brain)
{
    Fixed best;
    for (const auto& value : brain.m_threat)
        if (value.m_source == brain.m_target)
            best = value.m_damage;
    for (const auto& value : brain.m_threat)
        if (value.m_damage > best && hittable(world, value.m_source))
        {
            best            = value.m_damage;
            brain.m_target  = value.m_source;
        }
    brain.m_threat.clear();
}
// ChancePercent 只能出现在本行最后，避免前面的判断失败前就消耗随机数。
void assertChanceLast(const AiRule& rule)
{
    MG_ASSERT(rule.m_testCount <= rule.m_tests.size() && "AI rule has too many tests");
    for (uint8_t index = 0; index < rule.m_testCount; ++index)
        if (rule.m_tests[index].m_kind == AiCondition::ChancePercent)
            MG_ASSERT(index + 1 == rule.m_testCount && "ChancePercent must be the last test");
}
// 计算单项判断；只有 ChancePercent 会推进本场随机数。
bool passes(const AiTest& test, const AiFacts& facts, World& world)
{
    bool pass = false;
    switch (test.m_kind)
    {
    case AiCondition::Always:
        pass = true;
        break;
    case AiCondition::Alerted:
        pass = facts.m_alerted;
        break;
    case AiCondition::Provoked:
        pass = facts.m_provoked;
        break;
    case AiCondition::HasTarget:
        pass = facts.m_hasTarget;
        break;
    case AiCondition::TargetInSight:
        pass = facts.m_targetInSight;
        break;
    case AiCondition::TargetOutsideChase:
        pass = facts.m_targetOutsideChase;
        break;
    case AiCondition::SkillReady:
        pass = facts.m_skillReady;
        break;
    case AiCondition::LastActionIs:
        pass = static_cast<int32_t>(facts.m_lastAction) == test.m_value;
        break;
    case AiCondition::HpBelowPercent:
        pass = facts.m_hpPercent < test.m_value;
        break;
    case AiCondition::ChancePercent:
        pass = rng::draw(world, rng::DrawKind::AiChance) <= Fixed::fromInt(test.m_value);
        break;
    }
    return test.m_negate ? !pass : pass;
}
// 本行全部有效判断都成立才命中。
bool matches(const AiRule& rule, const AiFacts& facts, World& world)
{
    assertChanceLast(rule);
    for (uint8_t index = 0; index < rule.m_testCount; ++index)
        if (!passes(rule.m_tests[index], facts, world))
            return false;
    return true;
}
// 按配置展开怪物技能簿：普通技能组落 A..O，other 技能组落 H/I/J（醒/备用/待机）。
void buildMonsterSkillBook(World& world, Entity entity, const conf::battle::AiDef& ai)
{
    SkillBookComponent book;

    const auto addSlot = [&](int32_t firstId, const char* name) {
        if (firstId == -1)
            return;
        const int32_t slotIndex = static_cast<int32_t>(book.m_slots.size());
        book.m_slots.emplace_back(name);
        std::vector<int32_t> visited;
        int32_t id   = firstId;
        int32_t step = 1;
        const size_t start = book.m_skills.size();
        while (id != -1)
        {
            const auto* def = conf::Config::instance().find<conf::battle::SkillDef>(id);
            if (!def)
                break;
            if (std::find(visited.begin(), visited.end(), id) != visited.end())
            {
                MG_LOG_E("Cyclic next_skill configuration for skill {}", id);
                break;
            }
            visited.push_back(id);
            SkillRuntime rt;
            rt.m_slot         = slotIndex;
            rt.m_configId     = id;
            rt.m_step         = step;
            rt.m_stepCount    = 1;
            rt.m_link         = 1;
            rt.m_mode         = 1;
            rt.m_releaseMax   = def->charges == -1 ? 1 : std::max(1, def->charges);
            rt.m_releaseCount = rt.m_releaseMax;
            rt.m_coldMaxTime  = Fixed::fromInt(def->cdMs);
            rt.m_cd           = {};
            book.m_skills.push_back(std::move(rt));
            ++step;
            id = def->next;
        }
        for (size_t i = start; i < book.m_skills.size(); ++i)
            book.m_skills[i].m_stepCount = step - 1;
        if (start < book.m_skills.size())
        {
            for (size_t i = start; i < book.m_skills.size(); ++i)
                book.m_skills[i].m_finally = false;
            book.m_skills.back().m_finally = true;
        }
    };

    const size_t normalCount = std::min(ai.m_skillIds.size(), static_cast<size_t>('O' - 'A' + 1));
    for (size_t i = 0; i < normalCount; ++i)
        addSlot(firstValue(ai.m_skillIds, i, -1), std::string(1, static_cast<char>('A' + i)).c_str());
    for (size_t j = 0; j < ai.m_otherSkillIds.size() && j < 3; ++j)
        addSlot(firstValue(ai.m_otherSkillIds, j, -1), std::string(1, static_cast<char>('H' + j)).c_str());

    world.ecs().add(entity, std::move(book));
}
}  // namespace

namespace rules
{
int8_t pickRule(World& world, const AiFacts& facts, std::span<const AiRule> rules,
                std::span<const Fixed> ruleCooldownMs)
{
    if (facts.m_inAttack)
        return -1;
    for (size_t index = 0; index < rules.size(); ++index)
    {
        if (facts.m_movingRule >= 0 && static_cast<int8_t>(index) >= facts.m_movingRule)
            return -1;
        if (index < ruleCooldownMs.size() && ruleCooldownMs[index] > Fixed{})
            continue;
        if (matches(rules[index], facts, world))
            return static_cast<int8_t>(index);
    }
    return -1;
}

void tickRuleCooldowns(std::span<Fixed> ruleCooldownMs, Fixed dtMs)
{
    for (Fixed& remaining : ruleCooldownMs)
        if (remaining > Fixed{})
            remaining = remaining > dtMs ? remaining - dtMs : Fixed{};
}

void updateAgent(World& world, Entity entity)
{
    auto& ecs      = world.ecs();
    auto* brain    = ecs.tryGet<AiBrainComponent>(entity);
    if (!brain || !brain->m_enabled)
        return;
    const auto* row = conf::Config::instance().find<conf::battle::AiDef>(brain->m_aiId);
    if (!row)
        return;
    if (ecs.has<StatusFlagsComponent>(entity) && !ecs.get<StatusFlagsComponent>(entity).canControl())
        return;

    const Fixed dt = ecs.singleton<WorldClock>().m_dtMs;
    for (Fixed& interval : brain->m_skillIntervalMs)
        interval = std::max(Fixed{}, interval - dt);
    tickRuleCooldowns(brain->m_ruleCooldownMs, dt);

    // 每帧重写意图：清空上一帧的施法按键与自动施法标记，默认停止移动。
    auto& intent        = ecs.get<IntentComponent>(entity);
    intent.m_buttons.clear();
    intent.m_autoCast   = false;
    intent.m_move.m_mode     = MoveMode::Stop;
    intent.m_move.m_quadrant = ControlQuadrant::None;
    intent.m_move.m_angle    = 0;

    selectTarget(world, entity, *brain, *row);
    intent.m_target = brain->m_target;
    if (canAct(world, entity))
    {
        const AiFacts facts = gather(world, entity, *brain, *row);
        const int8_t idx    = pickRule(world, facts, kMonsterRules, brain->m_ruleCooldownMs);
        if (idx >= 0)
        {
            const auto& rule = kMonsterRules[static_cast<size_t>(idx)];
            const bool started = run(world, entity, *brain, rule.m_action, *row);
            if (rule.m_setsAlert)
                brain->m_alerted = true;
            brain->m_lastAction = started ? rule.m_action : AiAction::Idle;
            // 施法类动作打断移动：清掉移动规则，避免攻击期间仍续写移动意图。
            if (started && !isMoveAction(rule.m_action))
                brain->m_movingRule = -1;
            if (rule.m_cooldownMs > 0)
                brain->m_ruleCooldownMs[static_cast<size_t>(idx)] = Fixed::fromInt(rule.m_cooldownMs);
        }
        else if (brain->m_movingRule >= 0)
        {
            // 没有更高优先级规则抢占：延续移动，到达目标后停止。
            if (!continueMove(world, entity, *brain))
                brain->m_movingRule = -1;
        }
    }
    else
    {
        // 死亡/受击/失控时不能决策，终止在跑的移动。
        brain->m_movingRule = -1;
    }

    const Entity taunt = sneerTarget(world, entity);
    if (alive(world, taunt))
    {
        brain->m_target = taunt;
        return;
    }
    Fixed& elapsed = brain->m_changeTargetMs;
    elapsed += dt;
    if (elapsed >= Fixed::fromInt(10000))
    {
        elapsed -= Fixed::fromInt(10000);
        changeTarget(world, *brain);
    }
}

void addThreat(World& world, Entity entity, Entity source, Fixed damage)
{
    auto* brain = world.ecs().tryGet<AiBrainComponent>(entity);
    if (!brain)
        return;
    for (auto& record : brain->m_threat)
        if (record.m_source == source)
        {
            record.m_damage = std::max(Fixed{}, record.m_damage + damage);
            return;
        }
    brain->m_threat.push_back({source, std::max(Fixed{}, damage)});
}

void ensureBrain(World& world, Entity entity, int32_t aiDifficulty)
{
    if (world.ecs().has<AiBrainComponent>(entity))
        return;
    const auto& identity = world.ecs().get<IdentityComponent>(entity);
    const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
    if (!role || role->aiId.empty())
        return;
    const int32_t aiId = resolveAiId(*role, aiDifficulty);
    const auto* ai     = conf::Config::instance().find<conf::battle::AiDef>(aiId);
    if (!ai)
        return;
    if (!world.ecs().has<SkillBookComponent>(entity))
        buildMonsterSkillBook(world, entity, *ai);
    if (!world.ecs().has<SkillCastComponent>(entity))
        world.ecs().add(entity, SkillCastComponent{});
    if (!world.ecs().has<ActionTimelineComponent>(entity))
        world.ecs().add(entity, ActionTimelineComponent{});

    AiBrainComponent brain;
    brain.m_aiId   = aiId;
    brain.m_target = entity;
    // 参考语义：首次决策即用 skill_priority_level_cd 初始化各槽施放间隔，随后逐帧递减。
    for (const auto& values : ai->m_skillPriorityLevelCd)
        brain.m_skillIntervalMs.push_back(Fixed::fromInt(values.empty() ? ai->m_skillInterval : values.front()));
    world.ecs().add(entity, std::move(brain));
}
}  // namespace rules
}  // namespace mugen::sim::ai