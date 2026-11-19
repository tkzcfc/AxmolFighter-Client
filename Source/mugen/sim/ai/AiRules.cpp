#include "mugen/sim/ai/AiRules.h"
#include "mugen/sim/ai/AiComponents.h"
#include "mugen/sim/World.h"
#include "mugen/sim/base/Rng.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/sim/skill/SkillRules.h"
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
    for (const auto& values : row->skillPriorityLevelCd)
        if ((values.empty() ? row->skillInterval : values.front()) <= 0)
            return true;
    return false;
}
// 动作是否属于移动类（追击 / 巡逻 / 游走）。
bool isMoveAction(AiAction action)
{
    return action == AiAction::Chase || action == AiAction::Patrol || action == AiAction::Wander;
}
// 生命上限不为正时百分比为零。
int32_t hpPercent(const HealthComponent& health)
{
    if (health.maxHp <= Fixed{})
        return 0;
    return static_cast<int32_t>((health.hp * Fixed::fromInt(100) / health.maxHp).toInt());
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
    if (world.ecs().get<RoleStateComponent>(entity).state == RoleStateId::Death)
        return false;
    return !(world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).dead);
}
// 不可命中状态不参与普通 AI 目标选择。
bool hittable(World& world, Entity entity)
{
    if (!alive(world, entity))
        return false;
    if (world.ecs().has<HealthComponent>(entity) && !world.ecs().get<HealthComponent>(entity).beHitEnabled)
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
    for (const auto& entry : world.ecs().get<TauntTableComponent>(self).entries)
        if (entry.weight > bestWeight)
        {
            bestWeight = entry.weight;
            best       = entry.applicator;
        }
    return best;
}
// 受击、仇恨记录或有效嘲讽都算激怒。
bool provoked(World& world, Entity self, const AiBrainComponent& brain)
{
    if (role::isTobeHit(world.ecs().get<RoleStateComponent>(self).state))
        return true;
    if (!brain.threat.empty())
        return true;
    return alive(world, sneerTarget(world, self));
}
// 死亡、受击和失控时不能执行主动决策动作。
bool canAct(World& world, Entity self)
{
    const auto& state = world.ecs().get<RoleStateComponent>(self);
    if (state.state == RoleStateId::Death || role::isTobeHit(state.state))
        return false;
    return !world.ecs().has<StatusFlagsComponent>(self) ||
           world.ecs().get<StatusFlagsComponent>(self).canControl();
}
// 以固定创建顺序收集存活、启用且可命中的敌对阵营组角色。
std::vector<Entity> candidates(World& world, Entity self)
{
    std::vector<Entity> result;
    const int camp = world.ecs().get<IdentityComponent>(self).camp;
    world.ecs().each<IdentityComponent, HealthComponent>(
        [&](Entity other, const IdentityComponent& identity, const HealthComponent&) {
        if (world.ecs().has<InactiveComponent>(other) || world.ecs().has<DespawningComponent>(other))
            return;
        if (identity.kind != SpawnKind::Role)
            return;
        if (camp == identity.camp || campGroup(camp) == 0 || campGroup(camp) == campGroup(identity.camp))
            return;
        if (hittable(world, other))
            result.push_back(other);
    });
    return result;
}
// 先看已锁定目标再按创建顺序看候选者，用带符号的索敌区间；没有目标时返回自身哨兵。
Entity sightTarget(World& world, Entity self, const AiBrainComponent& brain, const conf::battle::AiDef& row)
{
    const FixedVec3 origin = world.ecs().get<TransformComponent>(self).position;
    const Entity locked    = brain.target;
    if (const auto* other = world.ecs().tryGet<TransformComponent>(locked))
        if (locked != self && contains(row.targetScopeX, origin.x - other->position.x) &&
            contains(row.targetScopeZ, origin.z - other->position.z))
            return locked;
    for (const Entity other : candidates(world, self))
    {
        const auto pos = world.ecs().get<TransformComponent>(other).position;
        if (contains(row.targetScopeX, origin.x - pos.x) && contains(row.targetScopeZ, origin.z - pos.z))
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
    if (bound(row.chaseScopeX, 0) == Fixed{} && bound(row.chaseScopeX, 1) == Fixed{} &&
        bound(row.chaseScopeZ, 0) == Fixed{} && bound(row.chaseScopeZ, 1) == Fixed{})
        return false;
    const auto selfPos = world.ecs().get<TransformComponent>(self).position;
    const Fixed x      = selfPos.x - other->position.x;
    const Fixed z      = selfPos.z - other->position.z;
    if (x == Fixed{} && z == Fixed{})
        return false;
    if (x != Fixed{} && z != Fixed{} && contains(row.chaseScopeX, absValue(x)) &&
        contains(row.chaseScopeZ, absValue(z)))
        return false;
    return true;
}
// 按地图四侧边界和角色半径修正目标点。
FixedVec3 calcPos(World& world, Entity self, FixedVec3 point)
{
    const auto& map    = world.ecs().singleton<MapRegion>();
    const Fixed radius = world.ecs().has<HitboxesComponent>(self) ? world.ecs().get<HitboxesComponent>(self).radius
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
        world.ecs().has<SkillCastComponent>(self) ? world.ecs().get<SkillCastComponent>(self).skill : kNoSkill;
    facts.alerted            = brain.alerted;
    facts.provoked           = provoked(world, self, brain);
    facts.hasTarget          = brain.target != self && world.ecs().tryGet<TransformComponent>(brain.target) != nullptr;
    facts.targetInSight      = sightTarget(world, self, brain, row) != self;
    facts.targetOutsideChase = facts.hasTarget && outsideChase(world, self, brain.target, row);
    facts.skillReady         = current != kNoSkill || skillReady(brain.skillIntervalMs, &row);
    facts.movingRule         = canAct(world, self) ? brain.movingRule : static_cast<int8_t>(-1);
    facts.lastAction         = facts.movingRule >= 0 ? AiAction::Idle : brain.lastAction;
    facts.hpPercent          = hpPercent(health);
    facts.inAttack           = state.state == RoleStateId::Attack;
    return facts;
}
// 写入一条朝向 targetPos 的移动意图，怪物沿既有位移推进（与玩家共用路径）。
// 怪物移动动作用行走态（"running" 动作，kWalkRate / kWalkScaleTime），与旧实现 AiMove 一致；
// 跑动态（"walk" 动作）仅玩家普通移动使用，怪物 motion 通常不含该动作。
void writeMoveIntent(World& world, Entity self, FixedVec3 to)
{
    auto& intent        = world.ecs().get<IntentComponent>(self);
    const FixedVec3 from = world.ecs().get<TransformComponent>(self).position;
    const Fixed dx = to.x - from.x, dz = to.z - from.z;
    if (dx == Fixed{} && dz == Fixed{})
    {
        intent.move.mode     = MoveMode::Stop;
        intent.move.quadrant = ControlQuadrant::None;
        intent.move.angle    = 0;
        intent.move.targetPos = to;
        return;
    }
    intent.move.mode = MoveMode::Walk;
    intent.move.targetPos = to;
    // 方向由象限承载，斜率角只给 0–90°（0 轴对齐、90000 纯垂直）。
    intent.move.quadrant = dx >= Fixed{} ? (dz >= Fixed{} ? ControlQuadrant::UpRight : ControlQuadrant::DownRight)
                                             : (dz >= Fixed{} ? ControlQuadrant::UpLeft : ControlQuadrant::DownLeft);
    const Fixed deg = FixedMath::atan2Deg(absValue(dz), absValue(dx));
    intent.move.angle = static_cast<int32_t>((deg * Fixed::fromInt(1000)).toInt());
}
// 怪物普通技能槽名：S0、S1、…，避免与醒/待机槽冲突。
std::string monsterSkillSlot(size_t index)
{
    return "S" + std::to_string(index);
}
// 面向目标：仅改水平朝向。
void faceTarget(World& world, Entity self, Entity target)
{
    if (target.value == 0 || target == self)
        return;
    const auto* other = world.ecs().tryGet<TransformComponent>(target);
    if (!other)
        return;
    auto& transform = world.ecs().get<TransformComponent>(self);
    const Fixed dx  = other->position.x - transform.position.x;
    if (dx != Fixed{})
    {
        transform.towardX = dx > Fixed{} ? 1 : -1;
        transform.vectorX = Fixed::fromInt(transform.towardX);
    }
}
// 通过技能槽预约醒/待机技能。first 为醒技能，否则待机技能。
bool castSpecial(World& world, Entity self, AiBrainComponent& brain, bool first, const conf::battle::AiDef& row)
{
    if (!canAct(world, self))
        return false;
    if (!first && candidates(world, self).empty())
        return false;
    const size_t otherIndex = first ? 0 : 2;
    if (firstValue(row.otherSkillIds, otherIndex, -1) == -1)
        return false;
    const int32_t next = skill::rules::prepareSlotCast(world, self, first ? "X" : "Z", !first);
    if (next == kNoSkill)
        return false;
    world.ecs().get<IntentComponent>(self).presetSkillIndex = next;
    if (first && brain.target == self)
    {
        const Entity spotted = sightTarget(world, self, brain, row);
        if (spotted != self)
            faceTarget(world, self, spotted);
    }
    else if (first || brain.target != self)
        faceTarget(world, self, brain.target);
    return true;
}
// 普通槽按优先级降序、原索引升序检查，成功后重置同优先级间隔。
bool castBest(World& world, Entity self, AiBrainComponent& brain, const conf::battle::AiDef& row)
{
    if (candidates(world, self).empty() || !canAct(world, self))
        return false;
    auto& intervals = brain.skillIntervalMs;
    if (intervals.empty())
        for (const auto& values : row.skillPriorityLevelCd)
            intervals.push_back(Fixed::fromInt(values.empty() ? row.skillInterval : values[0]));
    std::vector<size_t> ready;
    for (size_t i = 0; i < intervals.size(); ++i)
        if (intervals[i] <= Fixed{})
            ready.push_back(i);
    std::sort(ready.begin(), ready.end(), [&](size_t a, size_t b) {
        const auto pa = firstValue(row.skillPriorityLevel, a, 1), pb = firstValue(row.skillPriorityLevel, b, 1);
        return pa == pb ? a < b : pa > pb;
    });
    for (const size_t index : ready)
    {
        if (index >= row.skillIds.size() || firstValue(row.skillIds, index, -1) == -1)
            continue;
        const int32_t next = skill::rules::prepareSlotCast(world, self, monsterSkillSlot(index), true);
        if (next == kNoSkill)
            continue;
        auto& intent            = world.ecs().get<IntentComponent>(self);
        intent.presetSkillIndex = next;
        intent.autoCast         = true;
        faceTarget(world, self, brain.target);
        const auto priority = firstValue(row.skillPriorityLevel, index, 1);
        for (size_t i = 0; i < row.skillPriorityLevel.size(); ++i)
            if (firstValue(row.skillPriorityLevel, i, 1) == priority)
            {
                if (intervals.size() <= i)
                    intervals.resize(i + 1);
                intervals[i] = Fixed::fromInt(firstValue(row.skillPriorityLevelCd, i, row.skillInterval));
            }
        return true;
    }
    return false;
}
// 计算巡逻/追击/游走目标点。
FixedVec3 calcMoveGoal(World& world, Entity self, Entity targetEntity, AiAction action, const conf::battle::AiDef& row)
{
    const bool patrol = action == AiAction::Patrol;
    const bool chase  = action == AiAction::Chase;
    const FixedVec3 selfPos = world.ecs().get<TransformComponent>(self).position;
    FixedVec3 center        = selfPos;
    if (!patrol)
    {
        const auto* target = world.ecs().tryGet<TransformComponent>(targetEntity);
        if (!target)
            return selfPos;
        center = target->position;
    }
    const auto& xs = patrol ? row.patrolScopeX : row.chaseScopeX;
    const auto& zs = patrol ? row.patrolScopeZ : row.chaseScopeZ;
    const Fixed x  = chase ? bound(xs, 1) : rng::drawF(world, rng::DrawKind::AiWander, bound(xs, 0), bound(xs, 1));
    const Fixed z  = chase ? bound(zs, 1) : rng::drawF(world, rng::DrawKind::AiWander, bound(zs, 0), bound(zs, 1));
    center.x += !patrol && center.x > selfPos.x ? -x : x;
    center.z += !patrol && center.z > selfPos.z ? -z : z;
    return calcPos(world, self, center);
}
// 目标点是否落在地图可移动区域内。
bool inMapMoveRange(World& world, FixedVec3 pos)
{
    const auto& map = world.ecs().singleton<MapRegion>();
    if (map.moveRanges.empty())
        return true;
    for (const auto& range : map.moveRanges)
        if (pos.x >= range.x && pos.x <= range.x + range.width && pos.z >= range.y &&
            pos.z <= range.y + range.height)
            return true;
    return false;
}
// 抽取移动到达后的停留延迟。
Fixed drawMoveDelay(World& world, AiAction action, const conf::battle::AiDef& row)
{
    const auto& delays = action == AiAction::Patrol ? row.patrolDelayTime
                                                    : (action == AiAction::Chase ? row.chaseDelayTime : row.alertDelayTime);
    if (delays.empty())
        return {};
    if (delays.size() == 1)
        return Fixed::fromInt(delays[0]);
    return rng::drawF(world, rng::DrawKind::AiWander, Fixed::fromInt(delays[0]), Fixed::fromInt(delays[1]));
}
// 单帧行走步长；速度为 0 时取 1，避免永远到不了。
Fixed singleFrameWalkStep(World& world, Entity self)
{
    const Fixed dt       = world.ecs().singleton<WorldClock>().dtMs;
    const auto& identity = world.ecs().get<IdentityComponent>(self);
    const auto* role     = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
    Fixed step           = "1"_fx;
    if (role)
        step = role->velocity * conf::kWalkRate * dt;
    if (step < "1"_fx)
        step = "1"_fx;
    return step;
}
// 推进移动或到达后的停留等待；仍在进行返回 true。
bool stepMoveOrStay(World& world, Entity self, AiBrainComponent& brain, const conf::battle::AiDef& row)
{
    static_cast<void>(row);
    const Fixed dt = world.ecs().singleton<WorldClock>().dtMs;
    auto& intent   = world.ecs().get<IntentComponent>(self);
    if (!brain.moveComplete)
    {
        const auto* disp = world.ecs().tryGet<DisplacementComponent>(self);
        const bool hitWall = disp && disp->wall;
        auto& transform    = world.ecs().get<TransformComponent>(self);
        const Fixed dx     = brain.moveGoal.x - transform.position.x;
        const Fixed dz     = brain.moveGoal.z - transform.position.z;
        const Fixed step   = singleFrameWalkStep(world, self);
        if (hitWall || (absValue(dx) <= step && absValue(dz) <= step))
        {
            transform.position.x = brain.moveGoal.x;
            transform.position.z = brain.moveGoal.z;
            brain.moveComplete   = true;
            brain.moveWaitMs     = {};
        }
        else
        {
            writeMoveIntent(world, self, brain.moveGoal);
            return true;
        }
    }
    intent.move.mode     = MoveMode::Stop;
    intent.move.quadrant = ControlQuadrant::None;
    intent.move.angle    = 0;
    if (brain.alerted && brain.target != self)
        faceTarget(world, self, brain.target);
    brain.moveWaitMs += dt;
    return brain.moveWaitMs <= brain.moveDelayMs;
}
// 巡逻和游走先抽 X 再抽 Z；追击不消耗随机流。
bool move(World& world, Entity self, AiBrainComponent& brain, AiAction action, const conf::battle::AiDef& row)
{
    if (action == AiAction::Wander && brain.movingRule >= 0)
        return false;
    if (!canAct(world, self))
        return false;
    const bool patrol = action == AiAction::Patrol;
    const bool chase  = action == AiAction::Chase;
    const FixedVec3 selfPos = world.ecs().get<TransformComponent>(self).position;
    const auto& xs = patrol ? row.patrolScopeX : row.chaseScopeX;
    const auto& zs = patrol ? row.patrolScopeZ : row.chaseScopeZ;
    const Fixed x  = chase ? bound(xs, 1) : rng::drawF(world, rng::DrawKind::AiWander, bound(xs, 0), bound(xs, 1));
    const Fixed z  = chase ? bound(zs, 1) : rng::drawF(world, rng::DrawKind::AiWander, bound(zs, 0), bound(zs, 1));
    const Fixed radius = world.ecs().has<HitboxesComponent>(self) ? world.ecs().get<HitboxesComponent>(self).radius
                                                                  : Fixed{};
    if (!chase && absValue(x) < radius && absValue(z) < radius)
        return false;
    brain.moveDelayMs  = drawMoveDelay(world, action, row);
    brain.moveWaitMs   = {};
    brain.moveComplete = false;
    brain.moveGoal     = calcMoveGoal(world, self, brain.target, action, row);
    if (!inMapMoveRange(world, brain.moveGoal))
        brain.moveComplete = true;
    return stepMoveOrStay(world, self, brain, row);
}
// 延续既有移动或停留等待。
bool continueMove(World& world, Entity self, AiBrainComponent& brain, const conf::battle::AiDef& row)
{
    if (!canAct(world, self))
        return false;
    if (brain.movingRule == 5 && !brain.moveComplete)
    {
        brain.moveGoal = calcMoveGoal(world, self, brain.target, AiAction::Chase, row);
        if (!inMapMoveRange(world, brain.moveGoal))
            brain.moveComplete = true;
    }
    return stepMoveOrStay(world, self, brain, row);
}
// 执行命中规则的动作，返回动作是否真的启动。
bool run(World& world, Entity self, AiBrainComponent& brain, AiAction action, const conf::battle::AiDef& row)
{
    switch (action)
    {
    case AiAction::Idle:
        return false;
    case AiAction::CastWakeSkill:
        return castSpecial(world, self, brain, true, row);
    case AiAction::CastIdleSkill:
        return castSpecial(world, self, brain, false, row);
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
        brain.target = taunt;
        return;
    }
    if (!brain.alerted && !provoked(world, self, brain))
    {
        brain.target = sightTarget(world, self, brain, row);
        return;
    }
    if (hittable(world, brain.target))
        return;
    const auto enemies = candidates(world, self);
    if (enemies.empty())
    {
        brain.target = self;
        return;
    }
    brain.target = enemies[static_cast<size_t>(rng::drawN(world, rng::DrawKind::AiTarget, 0,
                                                            static_cast<int32_t>(enemies.size() - 1)))];
}
// 选择严格高于当前目标权重的有效攻击者，然后清空本周期记录。
void changeTarget(World& world, AiBrainComponent& brain)
{
    Fixed best;
    for (const auto& value : brain.threat)
        if (value.source == brain.target)
            best = value.damage;
    for (const auto& value : brain.threat)
        if (value.damage > best && hittable(world, value.source))
        {
            best            = value.damage;
            brain.target  = value.source;
        }
    brain.threat.clear();
}
// ChancePercent 只能出现在本行最后，避免前面的判断失败前就消耗随机数。
void assertChanceLast(const AiRule& rule)
{
    MG_ASSERT(rule.testCount <= rule.tests.size() && "AI rule has too many tests");
    for (uint8_t index = 0; index < rule.testCount; ++index)
        if (rule.tests[index].kind == AiCondition::ChancePercent)
            MG_ASSERT(index + 1 == rule.testCount && "ChancePercent must be the last test");
}
// 计算单项判断；只有 ChancePercent 会推进本场随机数。
bool passes(const AiTest& test, const AiFacts& facts, World& world)
{
    bool pass = false;
    switch (test.kind)
    {
    case AiCondition::Always:
        pass = true;
        break;
    case AiCondition::Alerted:
        pass = facts.alerted;
        break;
    case AiCondition::Provoked:
        pass = facts.provoked;
        break;
    case AiCondition::HasTarget:
        pass = facts.hasTarget;
        break;
    case AiCondition::TargetInSight:
        pass = facts.targetInSight;
        break;
    case AiCondition::TargetOutsideChase:
        pass = facts.targetOutsideChase;
        break;
    case AiCondition::SkillReady:
        pass = facts.skillReady;
        break;
    case AiCondition::LastActionIs:
        pass = static_cast<int32_t>(facts.lastAction) == test.value;
        break;
    case AiCondition::HpBelowPercent:
        pass = facts.hpPercent < test.value;
        break;
    case AiCondition::ChancePercent:
        pass = rng::draw(world, rng::DrawKind::AiChance) <= Fixed::fromInt(test.value);
        break;
    }
    return test.negate ? !pass : pass;
}
// 本行全部有效判断都成立才命中。
bool matches(const AiRule& rule, const AiFacts& facts, World& world)
{
    assertChanceLast(rule);
    for (uint8_t index = 0; index < rule.testCount; ++index)
        if (!passes(rule.tests[index], facts, world))
            return false;
    return true;
}
// 按配置展开怪物技能簿：普通技能组落 S0、S1、…，other 技能组落 X/W/Z（醒/备用/待机）。
void buildMonsterSkillBook(World& world, Entity entity, const conf::battle::AiDef& ai)
{
    SkillBookComponent book;

    const auto addSlot = [&](int32_t firstId, const std::string& name, const std::vector<int32_t>* aiIds) {
        if (firstId == -1)
            return;
        const int32_t slotIndex = static_cast<int32_t>(book.slots.size());
        book.slots.push_back(name);
        std::vector<int32_t> visited;
        int32_t id   = firstId;
        int32_t step = 1;
        const size_t start = book.skills.size();
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
            rt.slot         = slotIndex;
            rt.configId     = id;
            rt.step         = step;
            rt.stepCount    = 1;
            rt.link         = 1;
            rt.mode         = 1;
            rt.releaseMax   = def->charges == -1 ? 1 : std::max(1, def->charges);
            rt.releaseCount = rt.releaseMax;
            rt.coldMaxTime  = Fixed::fromInt(def->cdMs);
            rt.cd           = {};
            const int32_t aiId =
                aiIds && static_cast<size_t>(step - 1) < aiIds->size() ? (*aiIds)[static_cast<size_t>(step - 1)] : -1;
            skill::rules::initSkillAiRuntime(world, entity, rt, aiId, false);
            book.skills.push_back(std::move(rt));
            ++step;
            id = def->next;
        }
        for (size_t i = start; i < book.skills.size(); ++i)
            book.skills[i].stepCount = step - 1;
        if (start < book.skills.size())
        {
            for (size_t i = start; i < book.skills.size(); ++i)
                book.skills[i].finally = false;
            book.skills.back().finally = true;
        }
    };

    for (size_t i = 0; i < ai.skillIds.size(); ++i)
        addSlot(firstValue(ai.skillIds, i, -1), monsterSkillSlot(i),
                i < ai.skillAiIds.size() ? &ai.skillAiIds[i] : nullptr);
    static constexpr const char* kOtherSlots[] = {"X", "W", "Z"};
    for (size_t j = 0; j < ai.otherSkillIds.size() && j < 3; ++j)
        addSlot(firstValue(ai.otherSkillIds, j, -1), kOtherSlots[j], nullptr);

    world.ecs().add(entity, std::move(book));
}
}  // namespace

namespace rules
{
int8_t pickRule(World& world, const AiFacts& facts, std::span<const AiRule> rules,
                std::span<const Fixed> ruleCooldownMs)
{
    if (facts.inAttack)
        return -1;
    for (size_t index = 0; index < rules.size(); ++index)
    {
        if (facts.movingRule >= 0 && static_cast<int8_t>(index) >= facts.movingRule)
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
    if (!brain || !brain->enabled)
        return;
    const auto* row = conf::Config::instance().find<conf::battle::AiDef>(brain->aiId);
    if (!row)
        return;
    if (ecs.has<StatusFlagsComponent>(entity) && !ecs.get<StatusFlagsComponent>(entity).canControl())
        return;

    const Fixed dt = ecs.singleton<WorldClock>().dtMs;
    for (Fixed& interval : brain->skillIntervalMs)
        interval = std::max(Fixed{}, interval - dt);
    tickRuleCooldowns(brain->ruleCooldownMs, dt);

    // 每帧重写意图：清空上一帧的施法按键、预约技能与自动施法标记，默认停止移动。
    auto& intent        = ecs.get<IntentComponent>(entity);
    intent.buttons.clear();
    intent.autoCast         = false;
    intent.presetSkillIndex = -1;
    intent.move.mode        = MoveMode::Stop;
    intent.move.quadrant    = ControlQuadrant::None;
    intent.move.angle       = 0;

    selectTarget(world, entity, *brain, *row);
    intent.target = brain->target;
    if (canAct(world, entity))
    {
        AiFacts facts = gather(world, entity, *brain, *row);
        size_t limit  = facts.movingRule >= 0 ? static_cast<size_t>(facts.movingRule) : kMonsterRules.size();
        bool handled  = false;
        if (facts.inAttack)
            limit = 0;
        for (size_t idx = 0; idx < limit; ++idx)
        {
            if (idx < brain->ruleCooldownMs.size() && brain->ruleCooldownMs[idx] > Fixed{})
                continue;
            const auto& rule = kMonsterRules[idx];
            if (!matches(rule, facts, world))
                continue;
            if (rule.setsAlert && !brain->alerted)
            {
                brain->alerted = true;
                facts.alerted  = true;
                if (brain->target == entity)
                    selectTarget(world, entity, *brain, *row);
                facts.hasTarget          = brain->target != entity && ecs.tryGet<TransformComponent>(brain->target) != nullptr;
                facts.targetOutsideChase = facts.hasTarget && outsideChase(world, entity, brain->target, *row);
                intent.target            = brain->target;
            }
            if (rule.cooldownMs > 0)
                brain->ruleCooldownMs[idx] = Fixed::fromInt(rule.cooldownMs);
            const bool started = run(world, entity, *brain, rule.action, *row);
            if (started)
            {
                brain->lastAction = rule.action;
                if (isMoveAction(rule.action))
                    brain->movingRule = static_cast<int8_t>(idx);
                else
                {
                    brain->movingRule   = -1;
                    brain->moveComplete = false;
                }
                handled = true;
                break;
            }
            if (rule.setsAlert)
            {
                brain->movingRule   = -1;
                brain->moveComplete = false;
                facts.movingRule    = -1;
                limit               = kMonsterRules.size();
            }
        }
        if (!handled)
        {
            if (brain->movingRule >= 0)
            {
                if (!continueMove(world, entity, *brain, *row))
                {
                    brain->movingRule   = -1;
                    brain->moveComplete = false;
                    brain->lastAction   = AiAction::Idle;
                }
            }
            else
            {
                brain->lastAction = AiAction::Idle;
                if (brain->alerted && brain->target != entity)
                    faceTarget(world, entity, brain->target);
            }
        }
    }
    else
    {
        // 死亡/受击/失控时不能决策，终止在跑的移动。
        brain->movingRule   = -1;
        brain->moveComplete = false;
    }

    const Entity taunt = sneerTarget(world, entity);
    if (alive(world, taunt))
    {
        brain->target = taunt;
        return;
    }
    Fixed& elapsed = brain->changeTargetMs;
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
    for (auto& record : brain->threat)
        if (record.source == source)
        {
            record.damage = std::max(Fixed{}, record.damage + damage);
            return;
        }
    brain->threat.push_back({source, std::max(Fixed{}, damage)});
}

void ensureBrain(World& world, Entity entity, int32_t aiDifficulty)
{
    if (world.ecs().has<AiBrainComponent>(entity))
        return;
    const auto& identity = world.ecs().get<IdentityComponent>(entity);
    const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
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
    brain.aiId   = aiId;
    brain.target = entity;
    // 参考语义：首次决策即用 skill_priority_level_cd 初始化各槽施放间隔，随后逐帧递减。
    for (const auto& values : ai->skillPriorityLevelCd)
        brain.skillIntervalMs.push_back(Fixed::fromInt(values.empty() ? ai->skillInterval : values.front()));
    world.ecs().add(entity, std::move(brain));
}
}  // namespace rules
}  // namespace mugen::sim::ai