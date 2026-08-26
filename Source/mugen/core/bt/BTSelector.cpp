#include "mugen/core/bt/BTSelector.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTSelector::BTSelector() : m_currentIndex(-1) {}

BTSelector::~BTSelector() {}

bool BTSelector::onEnter(BTContext& ctx)
{
    if (!check(ctx))
        return false;

    const uint32_t count = static_cast<uint32_t>(m_children.size());
    for (uint32_t index = 0; index < count; ++index)
    {
        BTNode* child = m_children[index];
        if (child->enter(ctx))
        {
            m_currentIndex = static_cast<int32_t>(index);
            return true;
        }
        child->exit(ctx);
    }

    return false;
}

void BTSelector::onExit(BTContext& ctx)
{
    if (isRunning())
    {
        BTNode* child = m_children[m_currentIndex];
        if (child->isRunning())
            child->exit(ctx);
    }

    m_currentIndex = -1;
    Super::onExit(ctx);
}

BTStatus BTSelector::onUpdate(BTContext& ctx, int32_t dtMs)
{
    if (!conditionsHold(ctx))
    {
        BTNode* child = m_children[m_currentIndex];
        if (child->isRunning())
            child->exit(ctx);
        return BTStatus::Success;
    }

    return updateSelector(ctx, dtMs);
}

BTStatus BTSelector::updateSelector(BTContext& ctx, int32_t dtMs)
{
#define BT_SELECTOR_TEXTBOOK 0

    BTNode* child = m_children[m_currentIndex];
    child->update(ctx, dtMs);

#if BT_SELECTOR_TEXTBOOK
    if (child->isRunning())
        return BTStatus::Running;

    const bool succeeded = child->isSuccess();
    child->exit(ctx);
    if (succeeded)
        return BTStatus::Success;

    const uint32_t count = static_cast<uint32_t>(m_children.size());
    for (uint32_t index = static_cast<uint32_t>(m_currentIndex) + 1; index < count; ++index)
    {
        m_currentIndex       = static_cast<int32_t>(index);
        BTNode* childBrother = m_children[m_currentIndex];
        if (childBrother->enter(ctx))
            return BTStatus::Running;
        childBrother->exit(ctx);
    }
    return BTStatus::Failure;
#else
    if (child->isSuccess())
    {
        child->exit(ctx);

        const uint32_t count  = static_cast<uint32_t>(m_children.size());
        const uint32_t length = static_cast<uint32_t>(m_currentIndex) + count;
        for (uint32_t index = static_cast<uint32_t>(m_currentIndex) + 1; index < length; ++index)
        {
            m_currentIndex       = static_cast<int32_t>(index % count);
            BTNode* childBrother = m_children[m_currentIndex];
            if (childBrother->enter(ctx))
                return BTStatus::Running;
            childBrother->exit(ctx);
        }

        return BTStatus::Success;
    }

    // 这儿当前节点只应该是 Running,如果是 Failure,则本节点会一直处于 Running直到条件不成立
    MG_ASSERT(child->isRunning());
    // 如果去掉上面的宏,则应该判断是否执行失败,失败则要退出当前节点,并尝试下一个节点

    return BTStatus::Running;
#endif
#undef BT_SELECTOR_TEXTBOOK
}

NS_MG_END
