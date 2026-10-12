#pragma once

#include "BTDef.h"
#include "../reflect/Reflect.h"

namespace mugen
{
// 表示节点或守卫的生命周期结果。
enum class BTStatus : int8_t
{
    Readied,
    Running,
    Success,
    Failure
};

// 保存单个节点的可快照运行状态。
struct BTNodeState
{
    // 当前生命周期状态。
    BTStatus status      = BTStatus::Readied;
    // 串行组合节点当前子节点序号，-1 表示无。
    int16_t currentIndex = -1;
    // Parallel 节点仍在运行的子节点序号。
    std::vector<uint16_t> running;
    MG_REFLECT(BTNodeState, status, currentIndex, running)
};

// 保存树定义之外、可复制和序列化的实例状态。
template <class LeafState>
struct BTInstance
{
    static_assert(reflect::IsVariant<LeafState>::value && reflect::isSupported<LeafState>);
    // 按 DFS 节点顺序保存节点状态。
    std::vector<BTNodeState> nodes;
    // 按条件池顺序保存守卫状态。
    std::vector<BTStatus> conditions;
    // 按紧凑叶索引保存叶子私有状态。
    std::vector<LeafState> leaves;
    // 防止根停止诊断在同一轮执行中重复记录。
    bool rootStopLogged = false;
    MG_REFLECT(BTInstance, nodes, conditions, leaves, rootStopLogged)

    // 留给反序列化使用的空实例。
    BTInstance() = default;
    template <class Param>
    // 根据共享定义分配全部运行状态，并预留 Parallel 容量。
    explicit BTInstance(const BTDef<Param>& def)
        : nodes(def.topology().nodes().size())
        , conditions(def.conditionCount(), BTStatus::Readied)
        , leaves(def.topology().leafCount())
    {
        for (std::size_t id = 0; id < nodes.size(); ++id)
            if (def.topology().nodes()[id].type == BTNodeType::Parallel)
                nodes[id].running.reserve(def.topology().nodes()[id].childCount);
    }
};
}  // namespace mugen
