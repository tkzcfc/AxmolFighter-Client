#pragma once
#include "BuffRules.h"

namespace mugen::sim::buff
{
// 外部角色装配提供的一项效果，保持调用方顺序与来源。
struct LoadEntry
{
    // 效果配置及可选技能绑定、来源。
    int32_t m_id = -1;
    std::optional<int32_t> m_skill, m_origin;
};
}  // namespace mugen::sim::buff
