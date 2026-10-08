#pragma once

#include "BTRunner.h"

namespace mugen
{
template <class C, class P, class S>
bool BTRunner<C, P, S>::conditionsHold(C& ctx, uint32_t id, bool entering)
{
    const auto& node = m_def.topology().node(id);
    for (uint32_t i = node.firstCond; i < node.firstCond + node.condCount; ++i)
    {
        const auto& cond      = m_def.condition(i);
        const auto& callbacks = m_registry.condition(cond.condTypeId);
        const auto& param     = m_def.param(cond.paramIndex);
        if (!callbacks.check(ctx, param))
            return false;
        if (entering)
        {
            m_instance.conditions[i] = BTStatus::Running;
            callbacks.onEnter(ctx, param);
        }
    }
    return true;
}

template <class C, class P, class S>
bool BTRunner<C, P, S>::enterNode(C& ctx, uint32_t id)
{
    const auto& node = m_def.topology().node(id);
    auto& state      = m_instance.nodes[id];
    if (!conditionsHold(ctx, id, true))
    {
        state.status = BTStatus::Failure;
        return false;
    }
    switch (node.type)
    {
    case BTNodeType::Action:
        state.status = BTStatus::Running;
        m_registry.leaf(node.leafTypeId)
            .onEnter(ctx, m_def.param(node.paramIndex), m_instance.leaves[m_def.topology().leafIndex(id)]);
        return true;
    case BTNodeType::Selector:
    case BTNodeType::Sequence:
        for (uint16_t ordinal = 0; ordinal < node.childCount; ++ordinal)
        {
            if (enterNode(ctx, child(id, ordinal)))
            {
                state.currentIndex = static_cast<int16_t>(ordinal);
                state.status       = BTStatus::Running;
                return true;
            }
            exitNode(ctx, child(id, ordinal));
            if (node.type == BTNodeType::Sequence)
                break;
        }
        break;
    case BTNodeType::Parallel:
        state.running.clear();
        for (uint16_t ordinal = 0; ordinal < node.childCount; ++ordinal)
        {
            if (enterNode(ctx, child(id, ordinal)))
                state.running.push_back(ordinal);
            else
                exitNode(ctx, child(id, ordinal));
        }
        if (!state.running.empty())
        {
            state.status = BTStatus::Running;
            return true;
        }
        break;
    }
    state.status = BTStatus::Failure;
    return false;
}

template <class C, class P, class S>
void BTRunner<C, P, S>::exitChildren(C& ctx, uint32_t id)
{
    const auto& node = m_def.topology().node(id);
    auto& state      = m_instance.nodes[id];
    switch (node.type)
    {
    case BTNodeType::Selector:
    case BTNodeType::Sequence:
    {
        const auto current = child(id, static_cast<uint16_t>(state.currentIndex));
        if (m_instance.nodes[current].status == BTStatus::Running)
            exitNode(ctx, current);
        break;
    }
    case BTNodeType::Parallel:
        for (const auto ordinal : state.running)
        {
            const auto current = child(id, ordinal);
            if (m_instance.nodes[current].status == BTStatus::Running)
                exitNode(ctx, current);
        }
        state.running.clear();
        break;
    case BTNodeType::Action:
        break;
    }
}

template <class C, class P, class S>
void BTRunner<C, P, S>::exitNode(C& ctx, uint32_t id)
{
    const auto& node = m_def.topology().node(id);
    auto& state      = m_instance.nodes[id];
    if (node.type == BTNodeType::Action)
        m_registry.leaf(node.leafTypeId)
            .onExit(ctx, m_def.param(node.paramIndex), m_instance.leaves[m_def.topology().leafIndex(id)]);
    else if (state.status == BTStatus::Running)
        exitChildren(ctx, id);
    state.currentIndex = -1;
    state.running.clear();
    state.status = BTStatus::Readied;
    for (uint32_t offset = node.condCount; offset > 0; --offset)
    {
        const auto index = node.firstCond + offset - 1;
        if (m_instance.conditions[index] != BTStatus::Running)
            continue;
        const auto& cond = m_def.condition(index);
        m_registry.condition(cond.condTypeId).onExit(ctx, m_def.param(cond.paramIndex));
        m_instance.conditions[index] = BTStatus::Readied;
    }
}
}  // namespace mugen
