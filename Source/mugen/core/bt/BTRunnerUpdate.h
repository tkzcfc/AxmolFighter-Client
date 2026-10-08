#pragma once

#include "BTRunner.h"

namespace mugen
{
template <class C, class P, class S>
void BTRunner<C, P, S>::updateNode(C& ctx, uint32_t id)
{
    auto& state = m_instance.nodes[id];
    if (state.status != BTStatus::Running)
        return;
    const auto& node = m_def.topology().node(id);
    switch (node.type)
    {
    case BTNodeType::Action:
        state.status =
            m_registry.leaf(node.leafTypeId)
                .onUpdate(ctx, m_def.param(node.paramIndex), m_instance.leaves[m_def.topology().leafIndex(id)]);
        MG_ASSERT(state.status != BTStatus::Readied);
        break;
    case BTNodeType::Selector:
    case BTNodeType::Sequence:
    case BTNodeType::Parallel:
        state.status = updateComposite(ctx, id);
        break;
    }
}

template <class C, class P, class S>
BTStatus BTRunner<C, P, S>::updateComposite(C& ctx, uint32_t id)
{
    if (!conditionsHold(ctx, id, false))
    {
        exitChildren(ctx, id);
        return BTStatus::Success;
    }
    const auto& node = m_def.topology().node(id);
    if (node.type != BTNodeType::Parallel)
        return updateSerial(ctx, id);

    auto& running = m_instance.nodes[id].running;
    BTStatus next = BTStatus::Success;
    for (auto it = running.begin(); it != running.end();)
    {
        const auto current = child(id, *it);
        updateNode(ctx, current);
        if (m_instance.nodes[current].status == BTStatus::Running)
        {
            next = BTStatus::Running;
            ++it;
        }
        else
        {
            exitNode(ctx, current);
            it = running.erase(it);
        }
    }
    return next;
}

template <class C, class P, class S>
BTStatus BTRunner<C, P, S>::updateSerial(C& ctx, uint32_t id)
{
    const auto& node   = m_def.topology().node(id);
    auto& state        = m_instance.nodes[id];
    const auto current = child(id, static_cast<uint16_t>(state.currentIndex));
    const auto before  = m_instance.nodes[current].status;
    updateNode(ctx, current);
    if (m_instance.nodes[current].status != BTStatus::Success)
    {
#ifndef NDEBUG
        if (node.type == BTNodeType::Selector && before == BTStatus::Running &&
            m_instance.nodes[current].status == BTStatus::Failure)
            MG_LOG_W("BT selector ignored child failure: node={} child={}", id, current);
#else
        (void)before;
#endif
        return BTStatus::Running;
    }
    exitNode(ctx, current);
    switch (node.type)
    {
    case BTNodeType::Selector:
    {
        const uint32_t end = static_cast<uint32_t>(state.currentIndex) + node.childCount;
        for (uint32_t index = static_cast<uint32_t>(state.currentIndex) + 1; index < end; ++index)
        {
            state.currentIndex = static_cast<int16_t>(index % node.childCount);
            const auto sibling = child(id, static_cast<uint16_t>(state.currentIndex));
            if (enterNode(ctx, sibling))
                return BTStatus::Running;
            exitNode(ctx, sibling);
        }
        break;
    }
    case BTNodeType::Sequence:
        if (state.currentIndex + 1 < node.childCount)
        {
            ++state.currentIndex;
            const auto sibling = child(id, static_cast<uint16_t>(state.currentIndex));
            if (enterNode(ctx, sibling))
                return BTStatus::Running;
            exitNode(ctx, sibling);
        }
        break;
    case BTNodeType::Parallel:
    case BTNodeType::Action:
        MG_ASSERT(false && "Serial update requires a selector or sequence");
        break;
    }
    return BTStatus::Success;
}
}  // namespace mugen
