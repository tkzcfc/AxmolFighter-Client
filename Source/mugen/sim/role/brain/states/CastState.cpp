#include "CastState.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/attack/AttackRole.h"
#include "mugen/sim/skill/Skill.h"
#include "mugen/sim/status/Status.h"
#include <array>
#include <optional>
#include <utility>

namespace mugen::sim::role::brain::states
{
namespace
{
// 当前计划是否有正在执行的攻击叶。
bool leafLive(const CastPlan& plan)
{
    return plan.m_skill >= 0 && plan.m_index >= 0 && plan.m_index < static_cast<int32_t>(plan.m_actions.size());
}

// 用当前动作下标组装攻击叶参数。
attack::AttackLeafParam leafParam(const CastPlan& plan)
{
    return {plan.m_actions[static_cast<size_t>(plan.m_index)], plan.m_index + 1, plan.m_skill};
}

// 按控制角预览方向列，与施法开始时的方向结算一致。
int32_t previewToward(const SkillPool& pool, const SkillBase& skill, const conf::SkillAttackConfig& row)
{
    std::optional<Fixed> angle = pool.m_controlAngle;
    if ((skill.m_group == 3 || skill.m_group == 4) && skill.m_controlAngle)
        angle = skill.m_controlAngle;
    int32_t vector = skill.m_vector;
    if (angle)
    {
        constexpr std::array<int32_t, 8> vectors{4, 2, 1, 3, 5, 8, 6, 7};
        const auto sector = ((*angle / "22.5"_fx).floor().toInt() + 1) / 2;
        vector            = vectors.at(static_cast<size_t>((sector % 8 + 8) % 8));
    }
    const int32_t toward = vector > 5 ? vector - 5 : vector;
    const auto& actions  = row.actionIds;
    if (toward > 0 && static_cast<size_t>(toward) <= actions.size() && !actions[static_cast<size_t>(toward - 1)].empty() &&
        actions[static_cast<size_t>(toward - 1)].front() != -1)
        return toward;
    return 1;
}

// 方向列是否包含可播放的动作。
bool columnUsable(const std::vector<std::vector<int32_t>>& columns, int32_t toward)
{
    if (toward < 1 || static_cast<size_t>(toward) > columns.size())
        return false;
    const auto& actions = columns[static_cast<size_t>(toward - 1)];
    return !actions.empty() && actions.front() != -1;
}

// 退出当前攻击叶。
void exitLeaf(RoleBrain& brain, World& world, Entity entity)
{
    auto& plan = brain.m_cast;
    if (!leafLive(plan))
        return;
    RoleContext rc{world, entity};
    rc.setNode(static_cast<int32_t>(RoleBranch::Attack));
    attack::AttackAction::exit(rc, leafParam(plan), plan.m_leaf);
    plan.m_index = static_cast<int32_t>(plan.m_actions.size());
}

// 进入当前下标的攻击叶。
void enterLeaf(RoleBrain& brain, World& world, Entity entity)
{
    auto& plan = brain.m_cast;
    plan.m_leaf = {};
    RoleContext rc{world, entity};
    rc.setNode(static_cast<int32_t>(RoleBranch::Attack));
    attack::AttackAction::enter(rc, leafParam(plan), plan.m_leaf);
}

// 结束已开始的管线：先退攻击叶，再通知管线结束。
void closePipe(RoleBrain& brain, World& world, Entity entity)
{
    auto& plan = brain.m_cast;
    if (plan.m_skill < 0)
        return;
    const auto index = plan.m_skill;
    exitLeaf(brain, world, entity);
    plan = {};
    skill::dealWithCastPipeEnded(world, entity, index);
}

// 结束当前管线后，按当前技能决定续接新计划或复位技能。
StateStep endCast(RoleBrain& brain, World& world, Entity entity)
{
    const auto ended = brain.m_cast.m_skill;
    closePipe(brain, world, entity);
    auto& pool = world.ecs().get<SkillPool>(entity);
    if (pool.m_current != kNoSkill && pool.m_current != ended &&
        status::isAttackStatus(world.ecs().get<RoleStatus>(entity)) && CastState::enter(brain, world, entity))
        return StateStep::Running;
    skill::resetSkill(world, entity);
    return StateStep::Done;
}
}  // namespace

// 按当前技能、管线和方向列组装计划；没有可播放列时不施法。
bool CastState::enter(RoleBrain& brain, World& world, Entity entity)
{
    auto& pool = world.ecs().get<SkillPool>(entity);
    if (pool.m_current == kNoSkill)
        return false;
    const auto index = pool.m_current;
    const auto& skill = pool.m_skills.at(index);
    if (skill.m_pipe < 1 || skill.m_pipe > skill.m_releaseMax)
        return false;
    const auto* row = conf::Config::instance().find<conf::SkillAttackConfig>(skill.m_configId);
    if (!row)
        return false;
    const bool release = row->skillType == 1 && skill::canUpRelease(skill);
    const auto& columns = release ? row->upActionIds : row->actionIds;
    const auto toward = previewToward(pool, skill, *row);
    if (!columnUsable(columns, toward))
        return false;
    CastPlan plan;
    plan.m_skill   = index;
    plan.m_pipe    = skill.m_pipe;
    plan.m_release = release;
    plan.m_actions = columns[static_cast<size_t>(toward - 1)];
    plan.m_index   = 0;
    brain.m_cast   = std::move(plan);
    skill::dealWithCastPipeBegan(world, entity, index);
    if (row->skillType == 1)
        world.ecs().get<SkillPool>(entity).m_skills.at(index).m_enoughRelease = release;
    enterLeaf(brain, world, entity);
    return true;
}

// 推进攻击叶；同一技能的后续动作当场进入，换技能则等本次管线结束后再开新计划。
StateStep CastState::update(RoleBrain& brain, World& world, Entity entity)
{
    auto& plan = brain.m_cast;
    if (!leafLive(plan))
        return enter(brain, world, entity) ? StateStep::Running : StateStep::Done;
    // 技能已在打断窗口内切换：先结束当前管线（含桥接动作），再按当前技能续接或复位。
    const auto& pool = world.ecs().get<SkillPool>(entity);
    if (pool.m_current != plan.m_skill)
        return endCast(brain, world, entity);
    // 施放开始会轮换管线；轮换后不再等于进入时的管线（如耗尽到 0）则本次施放立即收束。
    if (pool.m_skills.at(plan.m_skill).m_pipe != plan.m_pipe)
        return endCast(brain, world, entity);
    RoleContext rc{world, entity};
    rc.setNode(static_cast<int32_t>(RoleBranch::Attack));
    const auto status = attack::AttackAction::update(rc, leafParam(plan), plan.m_leaf);
    if (status == BTStatus::Running)
        return StateStep::Running;
    // 列内还有动作则当场进入下一动作；打断切换时这里进入的即桥接动作。
    if (plan.m_index + 1 < static_cast<int32_t>(plan.m_actions.size()))
    {
        const auto next = plan.m_index + 1;
        exitLeaf(brain, world, entity);
        plan.m_index = next;
        enterLeaf(brain, world, entity);
        return StateStep::Running;
    }
    return endCast(brain, world, entity);
}

// 离开攻击分支时收束尚未结束的施法。
void CastState::exit(RoleBrain& brain, World& world, Entity entity)
{
    closePipe(brain, world, entity);
    skill::resetSkill(world, entity);
}

// 当前攻击叶处理位移事件。
bool CastState::onDisplacement(RoleContext& ctx, CastPlan& plan)
{
    if (!leafLive(plan))
        return false;
    return attack::AttackAction::onDisplacement(ctx, leafParam(plan), plan.m_leaf);
}
}  // namespace mugen::sim::role::brain::states
