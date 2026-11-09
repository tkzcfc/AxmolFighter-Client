// [2.2] BuffTickSystem
// 读：`BuffList`
// 写：`BuffList`（回收已请求移除的实例）
// 消费：—
// 产生：`BuffEvent`(Removed)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 BuffTickSystem：2.2 行，见 DESIGN.md §3.2。S1-B3 只处理缓存的移除请求（Removed 事件
// 延后一帧发出，对齐旧实现 next-update 回收），完整周期计时留到 S2。
void runBuffTick(World& world)
{
    world.ecs().each<BuffList>([&](Entity entity, BuffList& list) {
        for (auto it = list.m_instances.begin(); it != list.m_instances.end();)
        {
            if (it->m_removing)
            {
                BuffEvent event;
                event.m_entity = entity;
                event.m_buffId = it->m_id;
                event.m_serial = it->m_serial;
                event.m_phase  = BuffPhase::Removed;
                world.ecs().singleton<EventQueue<BuffEvent>>().push(event);
                it = list.m_instances.erase(it);
            }
            else
                ++it;
        }
    });
}
}  // namespace mugen::sim