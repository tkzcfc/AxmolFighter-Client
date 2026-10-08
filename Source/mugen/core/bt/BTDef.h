#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mugen
{
enum class BTNodeType : uint8_t
{
    Selector,
    Sequence,
    Parallel,
    Action
};

struct BTNodeDef
{
    BTNodeType type = BTNodeType::Action;
    // Derived offset into the direct-child index array, not into DFS nodes.
    uint32_t firstChild = 0;
    uint16_t childCount = 0;
    uint32_t firstCond  = 0;
    uint16_t condCount  = 0;
    uint16_t leafTypeId = 0;
    uint32_t paramIndex = 0;
};

struct BTCondDef
{
    uint16_t condTypeId = 0;
    uint32_t paramIndex = 0;
};

// Compiles DFS child counts into direct-child and dense leaf indices once.
class BTTopology
{
public:
    explicit BTTopology(std::vector<BTNodeDef> nodes);
    const BTNodeDef& node(uint32_t id) const { return m_nodes.at(id); }
    std::span<const BTNodeDef> nodes() const { return m_nodes; }
    uint32_t child(uint32_t id, uint16_t ordinal) const { return m_children[m_nodes[id].firstChild + ordinal]; }
    uint32_t leafIndex(uint32_t id) const { return m_leafIndices[id]; }
    uint32_t leafCount() const { return m_leafCount; }

private:
    uint32_t compile(uint32_t id);
    std::vector<BTNodeDef> m_nodes;
    std::vector<uint32_t> m_children;
    std::vector<uint32_t> m_leafIndices;
    uint32_t m_leafCount = 0;
};

// Param is a simulation-owned variant; the core never includes a concrete pool type.
template <class Param>
class BTDef
{
public:
    BTDef(std::vector<BTNodeDef> nodes, std::vector<BTCondDef> conditions, std::vector<Param> params)
        : m_topology(std::move(nodes)), m_conditions(std::move(conditions)), m_params(std::move(params))
    {
        std::size_t nextCond = 0;
        for (const auto& node : m_topology.nodes())
        {
            if (node.condCount != 0 && node.firstCond != nextCond)
                throw std::invalid_argument("BT conditions must be uniquely owned in DFS order");
            nextCond += node.condCount;
            if (nextCond > m_conditions.size() ||
                (node.type == BTNodeType::Action && node.paramIndex >= m_params.size()))
                throw std::invalid_argument("BT definition contains an invalid pool index");
        }
        if (nextCond != m_conditions.size())
            throw std::invalid_argument("BT definition contains unowned conditions");
        for (const auto& condition : m_conditions)
            if (condition.paramIndex >= m_params.size())
                throw std::invalid_argument("BT condition contains an invalid parameter index");
    }

    const BTTopology& topology() const { return m_topology; }
    const BTCondDef& condition(uint32_t id) const { return m_conditions[id]; }
    std::size_t conditionCount() const { return m_conditions.size(); }
    const Param& param(uint32_t id) const { return m_params[id]; }

private:
    BTTopology m_topology;
    std::vector<BTCondDef> m_conditions;
    std::vector<Param> m_params;
};
}  // namespace mugen
