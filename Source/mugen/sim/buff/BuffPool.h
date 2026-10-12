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
// 查询尚在池中的指定配置实例。
bool hasBuff(World& world, Entity entity, int32_t id);
// 校验免疫、减益免疫、冷却和场景限定。
bool buffCheck(World& world, Entity entity, const conf::BuffConfig& config, int32_t fightType);
// 更新添加冷却，调用者明确传入秒而非毫秒。
void updateCooldowns(BuffPool& pool, Fixed dtSeconds);
// 增删规则免疫集合，重复添加不累积引用。
void setImmune(BuffPool& pool, int32_t ruleId, bool immune);
// 根据现有同类层数计算概率，严格小于判定消耗调用方提供的抽样。
bool probabilityPass(const BuffPool& pool, const conf::BuffConfig& config, Fixed draw);
// 按配置规则和子类型返回首个实例的稳定序号。
std::optional<uint64_t> findByType(const BuffPool& pool, int32_t ruleId, int32_t subType);
// 按配置标识强制销毁，空标识销毁整个池并取消对应退出定时器。
void destroyBuff(World& world, Entity entity, std::optional<int32_t> id = {});
// 按规则主键强制销毁，使用逆插入顺序执行退出钩子。
void destroyBuffByRule(World& world, Entity entity, int32_t ruleId);
// 推进实体池，显式将世界毫秒步长转换为配置秒数。
void update(World& world, Entity entity);
// 向指定实体池添加实例，返回成功创建或匹配的稳定序号。
std::optional<uint64_t> addBuff(World& world, Entity entity, int32_t id,
                               const BuffContext& context, int32_t fightType, bool notify = true);
// 普通移除保留消失阶段，随后同步发送移除事件。
void removeBuff(World& world, Entity entity, int32_t id, bool all = false, bool notify = true);
// 加载角色自带效果，完成初始化后按配置目标分派。
void loadBuffs(World& world, Entity entity, const std::vector<LoadEntry>& additional = {});
// 施加器按配置选择目标并保留技能绑定与来源，指定目标仅作用于匹配阵营。
void addBuffForApplicator(World& world, Entity entity, int32_t id, const BuffContext& context,
                          int32_t fightType, bool notify = true);
}
