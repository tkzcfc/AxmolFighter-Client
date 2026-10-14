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
// 特殊能力持有者的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class AbilityHolder
{
public:
    // 构造指向指定世界实例中某个持有能力池实体的视图。
    AbilityHolder(World& world, Entity entity);
    // 读取条件配置并判断是否满足。
    bool checkConditions(const conf::SpecialAbilityConfig& config, const AbilityContext& context) const;
    // 将指定配置加入实体池；已存在的普通子类型保留原实例。
    std::optional<uint64_t> add(int32_t id, std::optional<int32_t> boundSkill = {});
    // 按事件顺序触发池中的全部已注册实例。
    void trigger(Event event, const AbilityContext& context = {});
    // 仅对指定配置创建或触发能力，供效果和 Buff 绑定使用。
    void triggerId(int32_t id, Event event, const AbilityContext& context = {});
    // 推进冷却、周期和结束计时。
    void update();
    // 删除全部实例，角色死亡时不保留能力状态。
    void clear();

private:
    // 在满足全部门槛后执行一次能力动作。
    void _activate(uint64_t serial, Event event, const AbilityContext& context);

    World& m_world;
    Entity m_entity{};
};
}  // namespace ability
}  // namespace mugen::sim
