#include "mugen/core/bt/BTParallel.h"
#include "mugen/core/bt/BTContext.h"

NS_MG_BEGIN

BTParallel::BTParallel() {}

BTParallel::~BTParallel() {}

void BTParallel::enter(BTContext& ctx)
{
    if (!check(ctx))
    {
        status = BTStatus::Failure;
        return;
    }

    m_running.clear();
    const uint32_t count = static_cast<uint32_t>(childCount());
    for (uint32_t index = 0; index < count; ++index)
    {
        BTNode* child = getChild(static_cast<int32_t>(index));
        child->enter(ctx);
        if (child->status == BTStatus::Running)
            m_running.push_back(index);
        else
            child->exit(ctx);
    }

    status = m_running.empty() ? BTStatus::Failure : BTStatus::Running;
}

void BTParallel::exit(BTContext& ctx)
{
    if (status == BTStatus::Running)
    {
        for (uint32_t index : m_running)
        {
            BTNode* child = getChild(static_cast<int32_t>(index));
            if (child->status == BTStatus::Running)
                child->exit(ctx);
        }
    }

    m_running.clear();
    status = BTStatus::Readied;
    Super::exit(ctx);
}

void BTParallel::update(BTContext& ctx, int32_t dtMs)
{
    if (status != BTStatus::Running)
        return;

    if (!conditionsHold(ctx))
    {
        for (uint32_t index : m_running)
        {
            BTNode* child = getChild(static_cast<int32_t>(index));
            if (child->status == BTStatus::Running)
                child->exit(ctx);
        }
        status = BTStatus::Success;
        return;
    }

    updateParallel(ctx, dtMs);
}

void BTParallel::updateParallel(BTContext& ctx, int32_t dtMs)
{
    status = BTStatus::Success;
    for (auto it = m_running.begin(); it != m_running.end();)
    {
        BTNode* child = getChild(static_cast<int32_t>(*it));
        child->update(ctx, dtMs);
        if (child->status == BTStatus::Running)
        {
            status = BTStatus::Running;
            ++it;
        }
        else
        {
            child->exit(ctx);
            it = m_running.erase(it);
        }
    }
}

NS_MG_END
