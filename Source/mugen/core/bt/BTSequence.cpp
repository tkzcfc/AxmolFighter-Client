#include "mugen/core/bt/BTSequence.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTSequence::BTSequence() {}

BTSequence::~BTSequence() {}

void BTSequence::enter(BTContext& ctx)
{
    if (!check(ctx))
    {
        status = BTStatus::Failure;
        return;
    }

    if (childCount() == 0)
    {
        status = BTStatus::Failure;
        return;
    }

    BTNode* child = getChild(0);
    child->enter(ctx);
    if (child->status == BTStatus::Running)
    {
        m_currentIndex = 0;
        status         = BTStatus::Running;
        return;
    }

    child->exit(ctx);
    status = BTStatus::Failure;
}

void BTSequence::exit(BTContext& ctx)
{
    if (status == BTStatus::Running)
    {
        BTNode* child = getChild(m_currentIndex);
        if (child->status == BTStatus::Running)
            child->exit(ctx);
    }

    m_currentIndex = -1;
    status         = BTStatus::Readied;
    Super::exit(ctx);
}

void BTSequence::update(BTContext& ctx, int32_t dtMs)
{
    if (status != BTStatus::Running)
        return;

    if (!conditionsHold(ctx))
    {
        BTNode* child = getChild(m_currentIndex);
        if (child->status == BTStatus::Running)
            child->exit(ctx);
        status = BTStatus::Success;
        return;
    }

    updateSequence(ctx, dtMs);
}

void BTSequence::updateSequence(BTContext& ctx, int32_t dtMs)
{
    BTNode* child = getChild(m_currentIndex);
    child->update(ctx, dtMs);

    if (child->status != BTStatus::Success)
        return;

    child->exit(ctx);
    if (static_cast<size_t>(m_currentIndex) != childCount() - 1)
    {
        ++m_currentIndex;
        BTNode* childBrother = getChild(m_currentIndex);
        childBrother->enter(ctx);
        if (childBrother->status == BTStatus::Running)
            return;
        childBrother->exit(ctx);
    }
    status = BTStatus::Success;
}

NS_MG_END
