#pragma once
#include "Events.h"
#include "WorldSetup.h"
#include "mugen/core/ecs/EventQueue.h"
#include "mugen/core/math/Random.h"
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
}  // namespace mugen::sim

// 新 sim 单例唯一登记表：时钟、随机数、全部事件队列、表现缓冲。
// 请求队列（Spawn..Room）与事实队列（SpawnEvent..DespawnEvent）都在帧末清空。
#define MG_SINGLETON_LIST(X)                                                                                   \
    X(WorldClock)                                                                                              \
    X(RngState)                                                                                                \
    X(PresentationEvents)                                                                                      \
    X(EventQueue<SpawnRequest>) X(EventQueue<DespawnRequest>) X(EventQueue<TransformRequest>)                  \
        X(EventQueue<DeathRequest>) X(EventQueue<RoomRequest>) X(EventQueue<SpawnEvent>)                       \
            X(EventQueue<ContactEvent>) X(EventQueue<HitEvent>) X(EventQueue<HpEvent>)                         \
                X(EventQueue<DeathEvent>) X(EventQueue<CastEvent>) X(EventQueue<ActionEvent>)                  \
                    X(EventQueue<StateEvent>) X(EventQueue<BuffEvent>) X(EventQueue<CueEvent>)                 \
                        X(EventQueue<RoomEvent>) X(EventQueue<DespawnEvent>)

namespace mugen::sim
{
#define MG_SIM_SINGLETON_TUPLE(T) \
    , std::tuple<T> {}
// 由登记表展开的 ECS 单例元组。
using Singletons = decltype(std::tuple_cat(std::tuple<> {} MG_SINGLETON_LIST(MG_SIM_SINGLETON_TUPLE)));
#undef MG_SIM_SINGLETON_TUPLE
}  // namespace mugen::sim
