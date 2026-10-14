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
// 更新添加冷却，调用者明确传入秒而非毫秒。
void updateCooldowns(BuffPool& pool, Fixed dtSeconds);
// 增删规则免疫集合，重复添加不累积引用。
void setImmune(BuffPool& pool, int32_t ruleId, bool immune);
// 根据现有同类层数计算概率，严格小于判定消耗调用方提供的抽样。
bool probabilityPass(const BuffPool& pool, const conf::BuffConfig& config, Fixed draw);
// 按配置规则和子类型返回首个实例的稳定序号。
std::optional<uint64_t> findByType(const BuffPool& pool, int32_t ruleId, int32_t subType);
}  // namespace mugen::sim::buff
