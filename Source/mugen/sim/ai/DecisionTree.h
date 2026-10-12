#pragma once

#include "mugen/core/reflect/Reflect.h"
#include <cstdint>
#include <vector>

namespace mugen::sim::ai
{
// 决策层返回值，独立于动作层的生命周期枚举。
enum class Result : uint8_t { Invalid, Success, Failure, Running };
// 决策树拥有的警戒状态，不等于角色动作状态位。
enum class AlertStatus : uint8_t { NoAlert, Alert };
// 决策树调用的怪物代理方法。
enum class Method : uint8_t
{
    CheckAttack, CheckAttackEnd, CheckHit, CheckHitEnd, CheckChase, CheckMoveEnd,
    CheckTarget, ExecuteRandAttack, ExecuteFirstAttack, ExecuteAttack,
    ExecuteChase, ExecuteAlert, ExecutePatrol
};
// 尚未完成的调用帧；游标保存已选择的分支。
struct DecisionFrame
{
    // XML 节点编号。
    uint8_t m_node = 0;
    // 下一子节点，IfElse 的零号子节点是条件。
    uint8_t m_child = 0;
    // 进入守卫只在首次进入时检查。
    bool m_entered = false;
    MG_REFLECT(DecisionFrame, m_node, m_child, m_entered)
};
// 可快照的决策状态；拓扑和回调均不写入快照。
struct DecisionState
{
    // 初始不警戒，只由树内赋值节点修改。
    AlertStatus m_alert = AlertStatus::NoAlert;
    // 根到正在运行的叶子的调用链。
    std::vector<DecisionFrame> m_stack;
    MG_REFLECT(DecisionState, m_alert, m_stack)
};
// 瞬时方法分派函数；用户数据由调用者持有，不进入状态。
using InvokeMethod = Result (*)(void*, Method);
// 推进手工构建的 Entity 决策树；完成的根在下次调用重新进入。
Result tickEntityTree(DecisionState& state, void* context, InvokeMethod invoke);
} // namespace mugen::sim::ai
