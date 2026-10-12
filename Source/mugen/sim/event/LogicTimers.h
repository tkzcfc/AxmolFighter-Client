#pragma once
#include "PresentationEvent.h"

namespace mugen::sim
{
class World;
// 表示定时器到期时向表现层发送的载荷。
struct EmitPresentation
{
    // 保存待发送的表现事件数据。
    PresentationPayload payload;
    MG_REFLECT(EmitPresentation, payload)
};
// 表示定时器到期时请求删除目标实体的载荷。
struct DestroyEntity
{
    // 保存需要删除的实体句柄。
    Entity target;
    MG_REFLECT(DestroyEntity, target)
};
// 限定逻辑定时器支持的到期行为类型。
// 受击延迟冻结，仅写入角色冻结时间。
struct HitFreeze
{
    // 到期时设置的冻结时长。
    Fixed m_duration;
    MG_REFLECT(HitFreeze, m_duration)
};
// 消失动作完成时退出指定 Buff 值实例。
struct BuffExit
{
    // 拥有者池内的稳定实例序号。
    uint64_t m_serial = 0;
    MG_REFLECT(BuffExit, m_serial)
};
// 出现动作完成后切换持续表现，不改变规则激活状态。
struct BuffContinue
{
    // 拥有者池内的稳定实例序号。
    uint64_t m_serial = 0;
    MG_REFLECT(BuffContinue, m_serial)
};
// 防守波次倒计时到期标记。
struct RoomWaveReady
{
    // 所属房间编号。
    int32_t m_roomId = 0;
    MG_REFLECT(RoomWaveReady, m_roomId)
};
using TimerPayload = std::variant<EmitPresentation, DestroyEntity, HitFreeze, BuffExit, BuffContinue, RoomWaveReady>;
// 保存单个逻辑定时器的所有可序列化状态。
struct LogicTimer
{
    // 唯一标识此定时器，取消操作使用该值。
    uint64_t id = 0;
    // 标识定时器所属实体，实体销毁时一并取消。
    Entity owner;
    // 累积距离上次触发的逻辑时间。
    Fixed elapsedMs;
    // 限定定时器两次触发之间的逻辑时间。
    Fixed intervalMs;
    // 指定触发后是否保留并继续计时。
    bool loop = false;
    // 保存触发时执行的具体行为。
    TimerPayload payload;
    MG_REFLECT(LogicTimer, id, owner, elapsedMs, intervalMs, loop, payload)
};
// 管理单场世界的逻辑定时器集合及递增标识。
struct LogicTimers
{
    // 保存按注册顺序排列的活动定时器。
    std::vector<LogicTimer> entries;
    // 为新定时器分配不重复标识。
    uint64_t nextId = 1;
    // 注册定时器并返回其标识，间隔必须非负。
    uint64_t add(Entity owner, Fixed intervalMs, bool loop, TimerPayload payload);
    // 按标识移除定时器。
    void cancel(uint64_t id);
    // 移除指定实体拥有的全部定时器。
    void cancelOwner(Entity owner);
    // 清零所有活动定时器的累计时间。
    void resetTimes();
    MG_REFLECT(LogicTimers, entries, nextId)
};
// 推进世界定时器并派发本帧到期载荷。
void updateLogicTimers(World& world);
}  // namespace mugen::sim
