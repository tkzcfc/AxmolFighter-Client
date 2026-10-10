#pragma once

#include <cstdint>
#include <span>
#include "mugen/core/StdC.h"
#include <utility>
#include <vector>

namespace mugen
{
// 定义行为树节点的执行类别，拓扑构建时必须保持有效枚举值。
enum class BTNodeType : uint8_t
{
    Selector,
    Sequence,
    Parallel,
    Action
};

// 描述 DFS 节点及其条件、参数和直接子节点范围的只读定义数据。
struct BTNodeDef
{
    // 指定节点的组合或叶子执行语义。
    BTNodeType type = BTNodeType::Action;
    // 指向直接子节点索引表，而不是 DFS 节点数组。
    uint32_t firstChild = 0;
    // 限定直接子节点索引表中连续子节点的数量。
    uint16_t childCount = 0;
    // 指向条件定义数组中的首个条件。
    uint32_t firstCond = 0;
    // 限定本节点拥有的连续条件数量。
    uint16_t condCount = 0;
    // 标识叶子节点在注册表中的回调类型。
    uint16_t leafTypeId = 0;
    // 指向叶子或条件使用的参数池条目。
    uint32_t paramIndex = 0;
};

// 描述节点守卫条件的注册类型与参数池引用。
struct BTCondDef
{
    // 标识条件在注册表中的回调类型。
    uint16_t condTypeId = 0;
    // 指向条件使用的参数池条目。
    uint32_t paramIndex = 0;
};

// 一次性把 DFS 关系编译为直接子节点表和紧凑叶子索引。
class BTTopology
{
public:
    // 接收 DFS 节点定义并验证、编译其派生拓扑数据。
    explicit BTTopology(std::vector<BTNodeDef> nodes);
    const BTNodeDef& node(uint32_t id) const { return m_nodes.at(id); }
    std::span<const BTNodeDef> nodes() const { return m_nodes; }
    uint32_t child(uint32_t id, uint16_t ordinal) const { return m_children[m_nodes[id].firstChild + ordinal]; }
    uint32_t leafIndex(uint32_t id) const { return m_leafIndices[id]; }
    uint32_t leafCount() const { return m_leafCount; }

private:
    // 递归编译当前 DFS 节点并返回下一个未消费节点索引。
    uint32_t compile(uint32_t id);
    // 保存按 DFS 顺序排列并已填充子节点偏移的节点定义。
    std::vector<BTNodeDef> m_nodes;
    // 保存每个组合节点按声明顺序排列的直接子节点索引。
    std::vector<uint32_t> m_children;
    // 将 DFS 节点索引映射为紧凑叶子状态索引。
    std::vector<uint32_t> m_leafIndices;
    // 记录叶子状态数组需要分配的总数量。
    uint32_t m_leafCount = 0;
};

// 保存共享且不可变的节点、条件和仿真参数定义，不依赖具体参数类型。
template <class Param>
class BTDef
{
public:
    // 接收所有定义池并验证其唯一归属和索引范围。
    BTDef(std::vector<BTNodeDef> nodes, std::vector<BTCondDef> conditions, std::vector<Param> params)
        : m_topology(std::move(nodes)), m_conditions(std::move(conditions)), m_params(std::move(params))
    {
        std::size_t nextCond = 0;
        for (const auto& node : m_topology.nodes())
        {
            if (node.condCount != 0 && node.firstCond != nextCond)
            {
                MG_LOG_E("BT conditions must be uniquely owned in DFS order");
                MG_ASSERT(false && "BT conditions must be uniquely owned in DFS order");
                return;
            }
            nextCond += node.condCount;
            if (nextCond > m_conditions.size() ||
                (node.type == BTNodeType::Action && node.paramIndex >= m_params.size()))
            {
                MG_LOG_E("BT definition contains an invalid pool index");
                MG_ASSERT(false && "BT definition contains an invalid pool index");
                return;
            }
        }
        if (nextCond != m_conditions.size())
        {
            MG_LOG_E("BT definition contains unowned conditions");
            MG_ASSERT(false && "BT definition contains unowned conditions");
            return;
        }
        for (const auto& condition : m_conditions)
            if (condition.paramIndex >= m_params.size())
            {
                MG_LOG_E("BT condition contains an invalid parameter index");
                MG_ASSERT(false && "BT condition contains an invalid parameter index");
                return;
            }
    }

    // 返回实例共享的已编译拓扑。
    const BTTopology& topology() const { return m_topology; }
    const BTCondDef& condition(uint32_t id) const { return m_conditions[id]; }
    std::size_t conditionCount() const { return m_conditions.size(); }
    const Param& param(uint32_t id) const { return m_params[id]; }

private:
    // 保存已编译的节点关系，实例不复制此拓扑。
    BTTopology m_topology;
    // 保存按 DFS 节点归属排列的不可变条件定义。
    std::vector<BTCondDef> m_conditions;
    // 保存叶子和条件共享的不可变参数池。
    std::vector<Param> m_params;
};
}  // namespace mugen
