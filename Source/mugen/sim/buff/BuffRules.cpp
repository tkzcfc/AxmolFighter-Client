#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"

namespace mugen::sim
{
namespace
{
// 应用一个常驻 op（Enter）。
void applyOp(World& world, Entity entity, const BuffOp& op)
{
    if (op.m_kind == BuffOpKind::Flag)
        world.ecs().get<StatusFlags>(entity).add(static_cast<mugen::ExtraState>(op.m_flag));
}
// 撤销一个常驻 op（Exit）。
void unapplyOp(World& world, Entity entity, const BuffOp& op)
{
    if (op.m_kind == BuffOpKind::Flag)
        world.ecs().get<StatusFlags>(entity).remove(static_cast<mugen::ExtraState>(op.m_flag));
}
}  // namespace

namespace buff::rules
{
// 常驻 op 降级：S1-B3 只识别霸体（ruleId 1），映射为 Flag(SuperArmor)。
std::vector<BuffOp> lowerOps(int32_t ruleId)
{
    if (ruleId == 1)
        return {{BuffOpKind::Flag, static_cast<int32_t>(mugen::ExtraState::SuperArmor)}};
    return {};
}

// 准入一个 Buff 实例（S1-B3 简化：subType -1 不查重，直接新增）。
void admit(World& world, Entity entity, int32_t buffId)
{
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(buffId);
    if (!config)
        return;
    if (!world.ecs().has<BuffList>(entity))
        world.ecs().add(entity, BuffList{});
    auto& list = world.ecs().get<BuffList>(entity);
    BuffInstance instance;
    instance.m_serial       = list.m_nextSerial++;
    instance.m_id           = buffId;
    instance.m_initialId    = buffId;
    instance.m_persistent   = lowerOps(config->ruleId);
    for (const auto& op : instance.m_persistent)
        applyOp(world, entity, op);
    instance.m_latched = !instance.m_persistent.empty();
    list.m_instances.push_back(instance);
    BuffEvent event;
    event.m_entity = entity;
    event.m_buffId = buffId;
    event.m_serial = instance.m_serial;
    event.m_phase  = BuffPhase::Added;
    world.ecs().singleton<EventQueue<BuffEvent>>().push(event);
}

// 移除指定 id 的全部实例：撤销常驻 op，标记下一帧回收（Removed 事件在 BuffTick 发出）。
void remove(World& world, Entity entity, int32_t buffId)
{
    if (!world.ecs().has<BuffList>(entity))
        return;
    auto& list = world.ecs().get<BuffList>(entity);
    for (auto& instance : list.m_instances)
    {
        if (instance.m_id == buffId && !instance.m_removing)
        {
            if (instance.m_latched)
                for (const auto& op : instance.m_persistent)
                    unapplyOp(world, entity, op);
            instance.m_latched = false;
            instance.m_removing = true;
        }
    }
}
}  // namespace buff::rules
}  // namespace mugen::sim
