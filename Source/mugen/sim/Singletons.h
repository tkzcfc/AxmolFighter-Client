#pragma once
#include "Events.h"
#include "WorldSetup.h"
#include "mugen/sim/motion/MapRegion.h"
#include "mugen/sim/room/KoSlowMotion.h"
#include "mugen/core/ecs/EventQueue.h"
#include "mugen/core/math/Random.h"
#include <map>
#include <tuple>

namespace mugen::sim
{
// 每场世界独有的固定帧时钟。
struct WorldClock
{
    // 已推进的逻辑帧序号。
    uint64_t m_frame = 0;
    // 从世界开始累计的逻辑毫秒。
    Fixed m_elapsedMs;
    // 当前逻辑帧的秒时长。
    Fixed m_dtSec = kLogicDtSec;
    // 当前逻辑帧的毫秒时长。
    Fixed m_dtMs = kLogicDtMs;
    MG_REFLECT(WorldClock, m_frame, m_elapsedMs, m_dtSec, m_dtMs)
};
// 每场世界独有的确定性随机数状态；逻辑代码只经它抽样。
struct RngState
{
    // 本场唯一的随机数发生器。
    Random m_random;
    MG_REFLECT(RngState, m_random)
};
// 当前世界的慢动作控制状态：外部定长慢放 + 房间 KO 曲线。
struct SlowMotion
{
    // 应用于逻辑时间的缩放倍率。
    Fixed m_scale = Fixed::fromInt(1);
    // 仍需保持缩放的逻辑帧数。
    uint32_t m_remainingFrames = 0;
    // 房间 KO 曲线独立于外部指定的定长慢放。
    room::KoSlowMotion m_ko;
    MG_REFLECT(SlowMotion, m_scale, m_remainingFrames, m_ko)
};
// 一条表现事件的最小记录；载荷在表现切片补全。
struct PresentationEvent
{
    // 事件产生的帧号。
    uint64_t m_frame = 0;
    // 帧内序号，每帧从 0 开始（R17）。
    uint32_t m_ordinal = 0;
    // 相关实体。
    Entity m_entity;
    MG_REFLECT(PresentationEvent, m_frame, m_ordinal, m_entity)
};
// 表现事件缓冲，保留到下一帧 ClockSystem 才清空，供表现层取走。
struct PresentationEvents
{
    // 上一帧产生的表现事件。
    std::vector<PresentationEvent> m_events;
    MG_REFLECT(PresentationEvents, m_events)
};
// 玩家槽位到角色实体的确定性映射。
struct PlayerSlots
{
    // 按玩家槽位保存其控制的实体句柄。
    std::map<uint32_t, Entity> m_entities;
    MG_REFLECT(PlayerSlots, m_entities)
};
// 房间级状态（B1 最小字段：房间标识、启用标志、暂停标志与等级增幅）。
struct RoomState
{
    // 当前房间标识。
    int32_t m_roomId = 0;
    // 是否启用房间流程。
    bool m_enabled = false;
    // 是否暂停推进世界。
    bool m_paused = false;
    // 副本等级增幅（怪物等级加成）。
    int32_t m_levelBonus = 0;
    // 是否已触发 KO（首波怪物清空后进入慢放结算）。
    bool m_ko = false;
    // 是否已检测到 Boss 清空、等待下一帧真正启动 KO 慢放（对齐旧实现「房间导演先于碰撞」的一帧延迟）。
    bool m_pendingKo = false;
    // 房间是否包含 Boss（roleType 4）。
    bool m_haveBoss = false;
    // 房间类型（0 表示无 KO 慢放）。
    int32_t m_roomType = 0;
    // 战斗规则类型（< 2 时才触发 KO 慢放）。
    int32_t m_battleRuleType = 0;
    MG_REFLECT(RoomState, m_roomId, m_enabled, m_paused, m_levelBonus, m_ko, m_pendingKo, m_haveBoss, m_roomType,
               m_battleRuleType)
};
// 生成登记表：各配置的 ordinal 计数（语义键 role:<configId>#<n>）。
struct SpawnRegistry
{
    // 配置标识 -> 已生成数量。
    std::map<int32_t, uint32_t> m_ordinalCounts;
    // 取并递增指定配置的下一个 ordinal（从 1 起）。
    uint32_t next(int32_t configId)
    {
        return ++m_ordinalCounts[configId];
    }
    MG_REFLECT(SpawnRegistry, m_ordinalCounts)
};
// 一条延迟动作（B2 仅顿帧延迟）。
struct ScheduledAction
{
    // 唯一标识。
    uint64_t m_id = 0;
    // 所属实体（销毁时一并取消）。
    Entity m_owner;
    // 到期剩余毫秒。
    Fixed m_dueMs;
    // 登记序号（到期时间相同时按序号稳定排序）。
    uint64_t m_seq = 0;
    // 顿帧延迟：到期时写入受击者的顿帧时长。
    Fixed m_freezeMs;
    MG_REFLECT(ScheduledAction, m_id, m_owner, m_dueMs, m_seq, m_freezeMs)
};
// 有序延迟动作表（取代旧 LogicTimers 的受击顿帧延迟）。
struct ScheduledActions
{
    // 按登记顺序保存的活动动作。
    std::vector<ScheduledAction> m_actions;
    // 下一个动作标识。
    uint64_t m_nextId = 1;
    // 下一个登记序号。
    uint64_t m_nextSeq = 0;
    // 登记顿帧延迟并返回标识；到期后写受击者顿帧。
    uint64_t add(Entity owner, Fixed dueMs, Fixed freezeMs)
    {
        m_actions.push_back({m_nextId, owner, dueMs, m_nextSeq++, freezeMs});
        return m_nextId++;
    }
    // 按标识取消（新命中取消旧的待生效顿帧）。
    void cancel(uint64_t id)
    {
        std::erase_if(m_actions, [id](const ScheduledAction& a) { return a.m_id == id; });
    }
    // 取消指定实体的全部动作。
    void cancelOwner(Entity owner)
    {
        std::erase_if(m_actions, [owner](const ScheduledAction& a) { return a.m_owner == owner; });
    }
    MG_REFLECT(ScheduledActions, m_actions, m_nextId, m_nextSeq)
};
}  // namespace mugen::sim

// 新 sim 单例唯一登记表：时钟、随机数、全部事件队列、表现缓冲、地图、槽位、房间与生成登记。
#define MG_SINGLETON_LIST(X)                                                                                   \
    X(WorldClock)                                                                                              \
    X(RngState)                                                                                                \
    X(SlowMotion)                                                                                              \
    X(PresentationEvents)                                                                                      \
    X(PlayerSlots)                                                                                             \
    X(MapRegion)                                                                                               \
    X(RoomState)                                                                                               \
    X(SpawnRegistry)                                                                                           \
    X(ScheduledActions)                                                                                        \
    X(EventQueue<SpawnRequest>) X(EventQueue<DespawnRequest>) X(EventQueue<TransformRequest>)                  \
        X(EventQueue<DeathRequest>) X(EventQueue<RoomRequest>) X(EventQueue<SpawnEvent>)                       \
            X(EventQueue<ContactEvent>) X(EventQueue<HitEvent>) X(EventQueue<HpEvent>)                         \
                X(EventQueue<DeathEvent>) X(EventQueue<CastEvent>) X(EventQueue<ActionEvent>)                  \
                    X(EventQueue<StateEvent>) X(EventQueue<BuffEvent>) X(EventQueue<CueEvent>)                 \
                        X(EventQueue<RoomEvent>) X(EventQueue<DespawnEvent>) X(EventQueue<DispEvent>)          \
                            X(EventQueue<StopEvent>)

namespace mugen::sim
{
#define MG_SIM_SINGLETON_TUPLE(T) \
    , std::tuple<T> {}
// 由登记表展开的 ECS 单例元组。
using Singletons = decltype(std::tuple_cat(std::tuple<> {} MG_SINGLETON_LIST(MG_SIM_SINGLETON_TUPLE)));
#undef MG_SIM_SINGLETON_TUPLE
}  // namespace mugen::sim
