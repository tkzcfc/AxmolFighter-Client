#pragma once

#include "BTInstance.h"

namespace mugen
{
// 叶子节点生命周期与位移事件的函数指针集合。
template <class BTCtx, class Param, class LeafState>
struct BTLeafCallbacks
{
    // 叶子开始运行时调用。
    void (*onEnter)(BTCtx&, const Param&, LeafState&);
    // 每帧更新叶子并返回新的状态。
    BTStatus (*onUpdate)(BTCtx&, const Param&, LeafState&);
    // 叶子退出时调用。
    void (*onExit)(BTCtx&, const Param&, LeafState&);
    // 可选的位移事件处理回调。
    bool (*onDisplacementEvent)(BTCtx&, const Param&, LeafState&) = nullptr;
};

// 守卫条件检查及生命周期函数指针集合。
template <class BTCtx, class Param>
struct BTCondCallbacks
{
    // 判断守卫当前是否通过。
    bool (*check)(BTCtx&, const Param&);
    // 守卫首次通过并进入节点时调用。
    void (*onEnter)(BTCtx&, const Param&);
    // 节点退出时调用。
    void (*onExit)(BTCtx&, const Param&);
};

// 按稳定类型编号保存叶子与守卫回调表。
template <class BTCtx, class Param, class LeafState>
class BTRegistry
{
public:
    using Leaf      = BTLeafCallbacks<BTCtx, Param, LeafState>;
    using Condition = BTCondCallbacks<BTCtx, Param>;
    // 接收按类型编号排列的不可变回调表。
    BTRegistry(std::vector<Leaf> leaves, std::vector<Condition> conditions)
        : m_leaves(std::move(leaves)), m_conditions(std::move(conditions))
    {}
    const Leaf& leaf(uint16_t id) const { return m_leaves.at(id); }
    const Condition& condition(uint16_t id) const { return m_conditions.at(id); }

private:
    // 叶子回调，索引必须匹配 leafTypeId。
    std::vector<Leaf> m_leaves;
    // 守卫回调，索引必须匹配 condTypeId。
    std::vector<Condition> m_conditions;
};
}  // namespace mugen
