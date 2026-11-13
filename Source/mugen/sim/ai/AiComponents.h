#pragma once
#include "mugen/sim/ai/AiRules.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/core/reflect/Reflect.h"
#include <array>
#include <vector>

namespace mugen::sim
{
// 按首次受击顺序保存一个攻击者的仇恨。
struct AiThreat
{
    Entity m_source;  // 攻击者句柄。
    Fixed m_damage;   // 累计伤害。
    MG_REFLECT(AiThreat, m_source, m_damage)
};
// 每个怪物独占的决策状态（DESIGN.md §4.2 AiBrain）。
struct AiBrainComponent
{
    // 已按难度解析的 AiDef 配置 id。
    int32_t m_aiId = 0;
    // 进入警戒后不再清除。
    bool m_alerted = false;
    // 上次启动的规则动作。
    ai::AiAction m_lastAction = ai::AiAction::Idle;
    // 每行规则剩余冷却，与 kMonsterRules 对齐。
    std::array<Fixed, 8> m_ruleCooldownMs{};
    // 各自动施放槽位的公共间隔。
    std::vector<Fixed> m_skillIntervalMs;
    // 换目标累计毫秒。
    Fixed m_changeTargetMs;
    // 稳定顺序的仇恨记录。
    std::vector<AiThreat> m_threat;
    // 正在执行的移动规则下标；-1 表示没有移动在跑。
    int8_t m_movingRule = -1;
    // 当前移动目标点（追击/巡逻/游走），延续移动时据此续写意图。
    FixedVec3 m_moveTarget;
    // 当前选定的敌对目标；哨兵为自身。
    Entity m_target;
    // 切换后是否开始更新（引导可暂停本角色的 AI）。
    bool m_enabled = true;
    MG_REFLECT(AiBrainComponent, m_aiId, m_alerted, m_lastAction, m_ruleCooldownMs, m_skillIntervalMs, m_changeTargetMs,
               m_threat, m_movingRule, m_moveTarget, m_target, m_enabled)
};
}  // namespace mugen::sim