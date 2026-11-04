#include "RoleCombo.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/buff/BuffComponents.h"
#include "mugen/conf/BattleConst.h"

#include <algorithm>
#include <utility>

namespace mugen::sim::role
{
namespace
{
// 根据连击数返回公式固定参数 (M, N)，忠实复刻参考实现的区间选择规则。
std::pair<Fixed, Fixed> formulaMN(int32_t comboNum)
{
    constexpr size_t count = conf::kComboFormulaCombo.size();
    for (size_t i = 0; i < count; ++i)
    {
        const int32_t threshold = conf::kComboFormulaCombo[i];
        if (threshold > comboNum)
            return {conf::kComboFormulaM[i], conf::kComboFormulaN[i]};
        if (threshold < comboNum && i == count - 1)
            return {conf::kComboFormulaM[i], conf::kComboFormulaN[i]};
    }
    return {"0"_fx, "0"_fx};
}
// 返回评价等级对应的每帧经验衰减速度。
Fixed stageSpeed(int32_t level)
{
    return conf::kComboStageSpeed[static_cast<size_t>(level - 1)];
}
}  // namespace

RoleCombo::RoleCombo(World& world, Entity entity) : m_world(world), m_entity(entity) {}

void RoleCombo::update()
{
    auto& ecs      = m_world.ecs();
    auto& state    = ecs.get<RoleHitState>(m_entity);
    const Fixed dt = ecs.singleton<WorldClock>().dtMs;
    // 连击超时重置，并清空已结算的击杀目标。
    if (state.m_curCombo > 0)
    {
        state.m_comboTimerMs += dt;
        if (state.m_comboTimerMs >= conf::kComboTimeMs)
        {
            state.m_curCombo     = 0;
            state.m_comboTimerMs = {};
            state.m_comboKilledTargets.clear();
        }
    }
    // 评价经验按当前等级每帧衰减，经验耗尽后清除评价等级。
    if (state.m_curEvaluate > 0)
    {
        recomputeEvaluate(state);
        if (state.m_evaluateExp <= Fixed{} && state.m_curEvaluate <= 1)
            state.m_curEvaluate = 0;
        else
            state.m_evaluateExp -= stageSpeed(state.m_curEvaluate);
        syncEvaluate(state);
    }
}

void RoleCombo::add(Fixed exp, Entity target)
{
    auto& ecs   = m_world.ecs();
    auto& state = ecs.get<RoleHitState>(m_entity);
    ++state.m_curCombo;
    state.m_maxCombo     = std::max(state.m_maxCombo, state.m_curCombo);
    state.m_comboTimerMs = {};
    const auto [m, n]    = formulaMN(state.m_curCombo);
    Fixed comboExp       = (exp + m) * n;
    // 击杀与眩晕特殊经验。
    Fixed specialExp;
    const auto& killed      = state.m_comboKilledTargets;
    const bool alreadyKilled = std::find(killed.begin(), killed.end(), target) != killed.end();
    if (Role(m_world, target).isDead() && !alreadyKilled)
    {
        specialExp = conf::kComboKillExp;
        state.m_comboKilledTargets.push_back(target);
    }
    else if (ecs.get<ExtraState>(target).active(mugen::ExtraState::Stun))
        specialExp = conf::kComboStunExp;
    // 首领命中经验倍率。
    const int32_t roleType = ecs.get<RoleIdentity>(target).roleType;
    Fixed typeRate         = "1"_fx;
    if (roleType == static_cast<int>(EntityRoleType::kBoss))
        typeRate = conf::kComboBossExpRate;
    comboExp = (comboExp + specialExp) * typeRate + state.m_evaluateExp;
    if (comboExp > conf::kComboStageExp.back())
        comboExp = conf::kComboStageExp.back();
    state.m_evaluateExp = comboExp;
    recomputeEvaluate(state);
    state.m_maxEvaluate = std::max(state.m_maxEvaluate, state.m_curEvaluate);
    syncEvaluate(state);
}

void RoleCombo::reset()
{
    auto& ecs   = m_world.ecs();
    auto& state = ecs.get<RoleHitState>(m_entity);
    state.m_curCombo          = 0;
    state.m_maxCombo          = 0;
    state.m_curEvaluate       = 0;
    state.m_maxEvaluate       = 0;
    state.m_evaluateExp       = {};
    state.m_comboTimerMs      = {};
    state.m_parameterExp      = "1"_fx;
    state.m_comboKilledTargets.clear();
    syncEvaluate(state);
}

void RoleCombo::recomputeEvaluate(RoleHitState& state)
{
    for (size_t i = 0; i < conf::kComboStageExp.size(); ++i)
    {
        if (conf::kComboStageExp[i] > state.m_evaluateExp)
        {
            state.m_curEvaluate = static_cast<int32_t>(i + 1);
            return;
        }
    }
}

void RoleCombo::syncEvaluate(RoleHitState& state)
{
    if (auto* external = m_world.ecs().tryGet<BuffExternalConditions>(m_entity))
        external->m_comboEvaluate = state.m_curEvaluate;
}
}  // namespace mugen::sim::role
