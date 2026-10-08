#include "BTDef.h"

#include <limits>

namespace mugen
{
BTTopology::BTTopology(std::vector<BTNodeDef> nodes) : m_nodes(std::move(nodes)), m_leafIndices(m_nodes.size())
{
    if (m_nodes.empty() || m_nodes.size() > std::numeric_limits<uint32_t>::max())
        throw std::invalid_argument("BT requires a nonempty representable node array");
    if (compile(0) != m_nodes.size())
        throw std::invalid_argument("BT DFS array contains disconnected nodes");
}

uint32_t BTTopology::compile(uint32_t id)
{
    if (id >= m_nodes.size())
        throw std::invalid_argument("BT DFS array is missing a child");
    auto& node = m_nodes[id];
    if (node.childCount > std::numeric_limits<int16_t>::max())
        throw std::invalid_argument("BT child count exceeds the runtime cursor range");
    switch (node.type)
    {
    case BTNodeType::Action:
        if (node.childCount != 0 || node.condCount != 0)
            throw std::invalid_argument("BT actions cannot own children or conditions");
        m_leafIndices[id] = m_leafCount++;
        break;
    case BTNodeType::Sequence:
        if (node.childCount == 0)
            throw std::invalid_argument("BT sequences require a child");
        break;
    case BTNodeType::Selector:
    case BTNodeType::Parallel:
        break;
    default:
        throw std::invalid_argument("BT node type is invalid");
    }
    node.firstChild = static_cast<uint32_t>(m_children.size());
    m_children.resize(m_children.size() + node.childCount);
    uint32_t next = id + 1;
    for (uint16_t ordinal = 0; ordinal < node.childCount; ++ordinal)
    {
        m_children[node.firstChild + ordinal] = next;
        next                                  = compile(next);
    }
    return next;
}
}  // namespace mugen
