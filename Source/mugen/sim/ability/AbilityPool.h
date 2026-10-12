#pragma once

#include "AbilityComponents.h"
#include "mugen/conf/tables/BuffTables.h"

namespace mugen::sim
{
class World;
namespace ability
{
// 特殊能力的配置事件编号。
enum class Event : int32_t
{
    BeforeCastSkill = 1, AfterCastSkill, BeforeHit, AfterHit, BeforeToBeHit, AfterToBeHit,
    BeforeCrazy, AfterCrazy, Time, InitAbility, GetUp, HitRepel, HitFly, ArtifactButton
};
// 读取条件配置并判断是否满足。
bool checkConditions(World&, Entity, const conf::SpecialAbilityConfig&, const AbilityContext&);
// 将指定配置加入实体池；已存在的普通子类型保留原实例。
std::optional<uint64_t> add(World&, Entity, int32_t id, std::optional<int32_t> boundSkill = {});
// 按事件顺序触发池中的全部已注册实例。
void trigger(World&, Entity, Event, const AbilityContext& = {});
// 仅对指定配置创建或触发能力，供效果和 Buff 绑定使用。
void triggerId(World&, Entity, int32_t id, Event, const AbilityContext& = {});
// 推进冷却、周期和结束计时。
void update(World&, Entity);
// 删除全部实例，角色死亡时不保留能力状态。
void clear(World&, Entity);
}
}  // namespace mugen::sim
