#pragma once

#include "BTInstance.h"

namespace mugen
{
template <class BTCtx, class Param, class LeafState>
struct BTLeafCallbacks
{
    void (*onEnter)(BTCtx&, const Param&, LeafState&);
    BTStatus (*onUpdate)(BTCtx&, const Param&, LeafState&);
    void (*onExit)(BTCtx&, const Param&, LeafState&);
};

template <class BTCtx, class Param>
struct BTCondCallbacks
{
    bool (*check)(BTCtx&, const Param&);
    void (*onEnter)(BTCtx&, const Param&);
    void (*onExit)(BTCtx&, const Param&);
};

template <class BTCtx, class Param, class LeafState>
class BTRegistry
{
public:
    using Leaf      = BTLeafCallbacks<BTCtx, Param, LeafState>;
    using Condition = BTCondCallbacks<BTCtx, Param>;
    BTRegistry(std::vector<Leaf> leaves, std::vector<Condition> conditions)
        : m_leaves(std::move(leaves)), m_conditions(std::move(conditions))
    {}
    const Leaf& leaf(uint16_t id) const { return m_leaves.at(id); }
    const Condition& condition(uint16_t id) const { return m_conditions.at(id); }

private:
    std::vector<Leaf> m_leaves;
    std::vector<Condition> m_conditions;
};
}  // namespace mugen
