#include "mugen/core/bt/BTSelector.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTSelector::BTSelector() : m_currentIndex(-1) {}

BTSelector::~BTSelector() {}

void BTSelector::enter(BTContext& ctx)
{
    if (!check(ctx))
    {
        status = BTStatus::Failure;
        return;
    }

    const uint32_t count = static_cast<uint32_t>(m_children.size());
    for (uint32_t index = 0; index < count; ++index)
    {
        BTNode* child = m_children[index];
        child->enter(ctx);
        MG_ASSERT(child->status != BTStatus::Readied);
        MG_ASSERT(child->status != BTStatus::Success);

        // 如果子节点返回 Running，则表示成功进入该节点，Selector 状态也设置为 Running
        if (child->status == BTStatus::Running)
        {
            m_currentIndex = static_cast<int32_t>(index);
            status         = BTStatus::Running;
            return;
        }
        // 节点进入失败，退出该节点
        child->exit(ctx);
    }

    status = BTStatus::Failure;
}

void BTSelector::exit(BTContext& ctx)
{
    if (status == BTStatus::Running)
    {
        BTNode* child = m_children[m_currentIndex];
        if (child->status == BTStatus::Running)
            child->exit(ctx);
    }

    m_currentIndex = -1;
    status         = BTStatus::Readied;
    Super::exit(ctx);
}

void BTSelector::update(BTContext& ctx, int32_t dtMs)
{
    if (status != BTStatus::Running)
        return;

    // 如果条件不成立，则退出当前正在运行的子节点，并将 Selector 状态设置为 Success (即选择器认为任务已成功完成)
    if (!conditionsHold(ctx))
    {
        BTNode* child = m_children[m_currentIndex];
        if (child->status == BTStatus::Running)
            child->exit(ctx);
        status = BTStatus::Success;
        return;
    }

    updateSelector(ctx, dtMs);
}

void BTSelector::updateSelector(BTContext& ctx, int32_t dtMs)
{
    BTNode* child = m_children[m_currentIndex];
    child->update(ctx, dtMs);

    // 如果当前子节点返回 Running，则表示当前节点还在执行中，Selector 也保持 Running 状态
    if (child->status != BTStatus::Success)
        return;

    // 当前子节点执行成功，退出该子节点，并尝试执行下一个子节点
    child->exit(ctx);

    const uint32_t count  = static_cast<uint32_t>(m_children.size());
    const uint32_t length = static_cast<uint32_t>(m_currentIndex) + count;
    for (uint32_t index = static_cast<uint32_t>(m_currentIndex) + 1; index < length; ++index)
    {
        // 计算下一个子节点的索引，使用取模运算确保索引在有效范围内循环
        m_currentIndex       = static_cast<int32_t>(index % count);
        BTNode* childBrother = m_children[m_currentIndex];
        childBrother->enter(ctx);
        MG_ASSERT(childBrother->status != BTStatus::Readied);
        MG_ASSERT(childBrother->status != BTStatus::Success);
        // 如果下一个子节点返回 Running，则表示成功进入该节点，Selector 状态也设置为 Running
        if (childBrother->status == BTStatus::Running)
            return;
        childBrother->exit(ctx);
    }

    // 如果没有成功进入其他任何子节点，则将 Selector 状态设置为 Success，表示选择器任务已成功完成
    status = BTStatus::Success;
}

NS_MG_END
