#pragma once

#include "BTDef.h"
#include "../reflect/Reflect.h"

namespace mugen
{
enum class BTStatus : int8_t
{
    Readied,
    Running,
    Success,
    Failure
};

struct BTNodeState
{
    BTStatus status      = BTStatus::Readied;
    int16_t currentIndex = -1;
    std::vector<uint16_t> running;
    MG_REFLECT(BTNodeState, status, currentIndex, running)
};

template <class LeafState>
struct BTInstance
{
    static_assert(reflect::IsVariant<LeafState>::value && reflect::isSupported<LeafState>);
    std::vector<BTNodeState> nodes;
    std::vector<BTStatus> conditions;
    std::vector<LeafState> leaves;
    bool rootStopLogged = false;
    MG_REFLECT(BTInstance, nodes, conditions, leaves, rootStopLogged)

    BTInstance() = default;
    template <class Param>
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
