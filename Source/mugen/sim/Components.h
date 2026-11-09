#pragma once
#include "mugen/core/reflect/Reflect.h"
#include <tuple>

namespace mugen::sim
{
// 标签组件：变身后休眠、跨房缓冲、房间挂起；所有系统遍历时跳过。
struct Inactive
{
    // 占位字节：零字段的 MG_REFLECT 会产生未使用参数告警，标签仍视为无数据。
    uint8_t m_tag = 0;
    MG_REFLECT(Inactive, m_tag)
};
// 标签组件：本帧待回收（R4）；打上后本帧其余系统一律跳过，物理回收在 FlushSystem。
struct Despawning
{
    // 占位字节：零字段的 MG_REFLECT 会产生未使用参数告警，标签仍视为无数据。
    uint8_t m_tag = 0;
    MG_REFLECT(Despawning, m_tag)
};
}  // namespace mugen::sim

// 新 sim 组件唯一登记表，顺序固定序列化类型编号；S0 只登记骨架用到的标签。
#define MG_COMPONENT_LIST(X) X(Inactive) X(Despawning)

namespace mugen::sim
{
#define MG_SIM_COMPONENT_TUPLE(T) \
    , std::tuple<T> {}
// 由登记表展开的 ECS 组件元组。
using Components = decltype(std::tuple_cat(std::tuple<> {} MG_COMPONENT_LIST(MG_SIM_COMPONENT_TUPLE)));
#undef MG_SIM_COMPONENT_TUPLE
}  // namespace mugen::sim
