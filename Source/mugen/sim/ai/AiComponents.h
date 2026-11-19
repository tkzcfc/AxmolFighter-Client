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
    Entity source;  // 攻击者句柄。
    Fixed damage;   // 累计伤害。
    MG_REFLECT(AiThreat, source, damage)
};
// 每个怪物独占的决策状态（DESIGN.md §4.2 AiBrain）。
struct AiBrainComponent
{
    // 已按难度解析的 AiDef 配置 id。
    int32_t aiId = 0;
    // 进入警戒后不再清除。
    bool alerted = false;
    // 上次启动的规则动作。
    ai::AiAction lastAction = ai::AiAction::Idle;
    // 每行规则剩余冷却，与 kMonsterRules 对齐。
    std::array<Fixed, 8> ruleCooldownMs{};
    // 各自动施放槽位的公共间隔。
    std::vector<Fixed> skillIntervalMs;
    // 换目标累计毫秒。
    Fixed changeTargetMs;
    // 稳定顺序的仇恨记录。
    std::vector<AiThreat> threat;
    // 正在执行的移动规则下标；-1 表示没有移动在跑。
    int8_t movingRule = -1;
    // 当前移动目标点（追击/巡逻/游走），延续移动时据此续写意图。
    FixedVec3 moveGoal;
    // 当前移动是否已到达，正在等待 stay delay。
    bool moveComplete = false;
    // stay delay 已累计毫秒。
    Fixed moveWaitMs;
    // 本次移动到达后的 stay delay 毫秒。
    Fixed moveDelayMs;
    // 当前选定的敌对目标；哨兵为自身。
    Entity target;
    // 切换后是否开始更新（引导可暂停本角色的 AI）。
    bool enabled = true;
    MG_REFLECT(AiBrainComponent, aiId, alerted, lastAction, ruleCooldownMs, skillIntervalMs, changeTargetMs,
               threat, movingRule, moveGoal, moveComplete, moveWaitMs, moveDelayMs, target, enabled)
};
}  // namespace mugen::sim