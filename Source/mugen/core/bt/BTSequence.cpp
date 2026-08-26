#include "mugen/core/bt/BTSequence.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTSequence::BTSequence() : m_currentIndex(-1) {}

BTSequence::~BTSequence() {}

bool BTSequence::onEnter(BTContext& ctx)
{
    if (!check(ctx))
        return false;

    if (childCount() == 0)
    {
        // 不应该存在没有子节点的 Sequence，至少应该有一个子节点
        MG_ASSERT(false);
        return false;
    }

    BTNode* child = getChild(0);
    if (child->enter(ctx))
    {
        m_currentIndex = 0;
        return true;
    }

    child->exit(ctx);
    return false;
}

void BTSequence::onExit(BTContext& ctx)
{
    if (isRunning())
    {
        BTNode* child = getChild(m_currentIndex);
        if (child->isRunning())
            child->exit(ctx);
    }

    m_currentIndex = -1;
    Super::onExit(ctx);
}

BTStatus BTSequence::onUpdate(BTContext& ctx, int32_t dtMs)
{
    if (!conditionsHold(ctx))
    {
        BTNode* child = getChild(m_currentIndex);
        if (child->isRunning())
            child->exit(ctx);
        return BTStatus::Success;
    }

    return updateSequence(ctx, dtMs);
}

BTStatus BTSequence::updateSequence(BTContext& ctx, int32_t dtMs)
{
    BTNode* child = getChild(m_currentIndex);
    child->update(ctx, dtMs);

    if (!child->isSuccess())
        return BTStatus::Running;

    child->exit(ctx);
    if (static_cast<size_t>(m_currentIndex) < childCount() - 1)
    {
        ++m_currentIndex;
        BTNode* childBrother = getChild(m_currentIndex);
        if (childBrother->enter(ctx))
            return BTStatus::Running;
        childBrother->exit(ctx);
    }
    return BTStatus::Success;
}

NS_MG_END
