#pragma once

#include "BTRegistry.h"

namespace mugen
{
// 瞬时行为树执行视图，禁止存入组件或快照。
template <class BTCtx, class Param, class LeafState>
class BTRunner
{
public:
    // 绑定共享定义、回调表与可变实例状态。
    BTRunner(const BTDef<Param>& def,
             const BTRegistry<BTCtx, Param, LeafState>& registry,
             BTInstance<LeafState>& instance)
        : m_def(def), m_registry(registry), m_instance(instance)
    {}

    // 从根节点进入；无法进入时记录一次根停止错误。
    bool enter(BTCtx& ctx)
    {
        m_instance.rootStopLogged = false;
        const bool running        = enterNode(ctx, 0);
        if (!running)
            reportStopped();
        return running;
    }
    // 推进运行中的根节点；根结束后返回 false。
    bool update(BTCtx& ctx)
    {
        if (m_instance.nodes[0].status != BTStatus::Running)
            return false;
        updateNode(ctx, 0);
        if (m_instance.nodes[0].status == BTStatus::Running)
            return true;
        reportStopped();
        return false;
    }
    // 退出根节点及其仍在运行的子节点。
    void exit(BTCtx& ctx) { exitNode(ctx, 0); }

private:
    // 进入节点并初始化守卫与子状态。
    bool enterNode(BTCtx& ctx, uint32_t id);
    // 逆序退出节点守卫并复位节点状态。
    void exitNode(BTCtx& ctx, uint32_t id);
    // 退出组合节点当前仍在运行的子节点。
    void exitChildren(BTCtx& ctx, uint32_t id);
    // 按节点类别推进单个运行节点。
    void updateNode(BTCtx& ctx, uint32_t id);
    // 检查守卫后更新组合节点。
    BTStatus updateComposite(BTCtx& ctx, uint32_t id);
    // 更新 Selector 或 Sequence 当前子节点。
    BTStatus updateSerial(BTCtx& ctx, uint32_t id);
    // 检查节点守卫；进入时同步触发守卫进入回调。
    bool conditionsHold(BTCtx& ctx, uint32_t id, bool entering);
    uint32_t child(uint32_t id, uint16_t ordinal) const { return m_def.topology().child(id, ordinal); }
    // 根节点首次停止时只记录一次诊断。
    void reportStopped()
    {
        if (!m_instance.rootStopLogged)
        {
            MG_LOG_E("BT root stopped: status={}", static_cast<int>(m_instance.nodes[0].status));
            m_instance.rootStopLogged = true;
        }
    }

    // 共享且不可变的树定义。
    const BTDef<Param>& m_def;
    // 叶子与守卫的函数指针注册表。
    const BTRegistry<BTCtx, Param, LeafState>& m_registry;
    // 本次执行所修改的值类型状态。
    BTInstance<LeafState>& m_instance;
};
}  // namespace mugen

#include "BTRunnerLifecycle.h"  // IWYU pragma: export
#include "BTRunnerUpdate.h"     // IWYU pragma: export
