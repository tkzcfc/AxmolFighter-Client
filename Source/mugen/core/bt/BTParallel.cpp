#include "mugen/core/bt/BTParallel.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTParallel::BTParallel() {}

BTParallel::~BTParallel() {}

bool BTParallel::onEnter(BTContext& ctx)
{
    if (!check(ctx))
        return false;

    m_running.clear();
    const uint32_t count = static_cast<uint32_t>(childCount());
    for (uint32_t index = 0; index < count; ++index)
    {
        BTNode* child = getChild(static_cast<int32_t>(index));
        if (child->enter(ctx))
            m_running.push_back(index);
        else
            child->exit(ctx);
    }

    return !m_running.empty();
}

void BTParallel::onExit(BTContext& ctx)
{
    if (isRunning())
    {
        for (uint32_t index : m_running)
        {
            BTNode* child = getChild(static_cast<int32_t>(index));
            if (child->isRunning())
                child->exit(ctx);
        }
    }

    m_running.clear();
    Super::onExit(ctx);
}

BTStatus BTParallel::onUpdate(BTContext& ctx, int32_t dtMs)
{
    if (!conditionsHold(ctx))
    {
        for (uint32_t index : m_running)
        {
            BTNode* child = getChild(static_cast<int32_t>(index));
            if (child->isRunning())
                child->exit(ctx);
        }
        m_running.clear();
        return BTStatus::Success;
    }

    return updateParallel(ctx, dtMs);
}

BTStatus BTParallel::updateParallel(BTContext& ctx, int32_t dtMs)
{
    BTStatus next = BTStatus::Success;
    for (auto it = m_running.begin(); it != m_running.end();)
    {
        BTNode* child = getChild(static_cast<int32_t>(*it));
        child->update(ctx, dtMs);
        if (child->isRunning())
        {
            next = BTStatus::Running;
            ++it;
        }
        else
        {
            child->exit(ctx);
            it = m_running.erase(it);
        }
    }
    return next;
}

NS_MG_END
