#include "BTDef.h"

#include <limits>

namespace mugen
{
// 构造时验证完整 DFS 树，并生成后续运行所需的派生索引。
BTTopology::BTTopology(std::vector<BTNodeDef> nodes) : m_nodes(std::move(nodes)), m_leafIndices(m_nodes.size())
{
    if (m_nodes.empty() || m_nodes.size() > std::numeric_limits<uint32_t>::max())
    {
        MG_LOG_E("BT requires a nonempty representable node array");
        MG_ASSERT(false && "BT requires a nonempty representable node array");
        return;
    }
    if (compile(0) != m_nodes.size())
    {
        MG_LOG_E("BT DFS array contains disconnected nodes");
        MG_ASSERT(false && "BT DFS array contains disconnected nodes");
    }
}

// 编译单个 DFS 子树，发现无效拓扑时停止继续读取节点数组。
uint32_t BTTopology::compile(uint32_t id)
{
    if (id >= m_nodes.size())
    {
        MG_LOG_E("BT DFS array is missing a child");
        MG_ASSERT(false && "BT DFS array is missing a child");
        return static_cast<uint32_t>(m_nodes.size());
    }
    auto& node = m_nodes[id];
    if (node.childCount > std::numeric_limits<int16_t>::max())
    {
        MG_LOG_E("BT child count exceeds the runtime cursor range");
        MG_ASSERT(false && "BT child count exceeds the runtime cursor range");
        return static_cast<uint32_t>(m_nodes.size());
    }
    switch (node.type)
    {
    case BTNodeType::Action:
        if (node.childCount != 0 || node.condCount != 0)
        {
            MG_LOG_E("BT actions cannot own children or conditions");
            MG_ASSERT(false && "BT actions cannot own children or conditions");
            return static_cast<uint32_t>(m_nodes.size());
        }
        m_leafIndices[id] = m_leafCount++;
        break;
    case BTNodeType::Sequence:
        if (node.childCount == 0)
        {
            MG_LOG_E("BT sequences require a child");
            MG_ASSERT(false && "BT sequences require a child");
            return static_cast<uint32_t>(m_nodes.size());
        }
        break;
    case BTNodeType::Selector:
    case BTNodeType::Parallel:
        break;
    default:
        MG_LOG_E("BT node type is invalid");
        MG_ASSERT(false && "BT node type is invalid");
        return static_cast<uint32_t>(m_nodes.size());
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
