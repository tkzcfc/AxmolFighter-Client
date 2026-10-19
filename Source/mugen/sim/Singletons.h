#pragma once
#include "WorldSetup.h"
#include "event/LogicTimers.h"
#include "input/FrameInput.h"
#include "mugen/core/math/Random.h"
#include "motion/MapRegion.h"
#include "room/KoSlowMotion.h"
#include "mugen/conf/tables/MapTables.h"
#include <tuple>

namespace mugen::sim
{
// 每场世界独有的固定帧时钟，不能共享到其他战斗实例。
struct WorldClock
{
    // 已推进的逻辑帧序号。
    uint64_t frame = 0;
    // 从世界开始累计的逻辑毫秒。
    Fixed elapsedMs;
    // 当前逻辑帧的秒时长。
    Fixed dtSec = kLogicDtSec;
    // 当前逻辑帧的毫秒时长。
    Fixed dtMs = kLogicDtMs;
    // 参与世界快照的时钟字段。
    MG_REFLECT(WorldClock, frame, elapsedMs, dtSec, dtMs)
};
// 每场世界独有的确定性随机数状态。
struct RngState
{
    // 本场唯一的随机数发生器。
    Random random;
    // 参与世界快照的随机数状态。
    MG_REFLECT(RngState, random)
};
// 当前世界的慢动作控制状态。
struct SlowMotion
{
    // 应用于逻辑时间的缩放倍率。
    Fixed scale = Fixed::fromInt(1);
    // 仍需保持缩放的逻辑帧数。
    uint32_t remainingFrames = 0;
    // 房间 KO 曲线独立于外部指定的定长慢放。
    room::KoSlowMotion m_ko;
    // 参与世界快照的慢动作字段。
    MG_REFLECT(SlowMotion, scale, remainingFrames, m_ko)
};
// 玩家槽位到角色实体的确定性映射。
struct PlayerSlots
{
    // 按玩家槽位保存其控制的实体句柄。
    std::map<uint32_t, Entity> entities;
    // 参与世界快照的槽位映射。
    MG_REFLECT(PlayerSlots, entities)
};
// 房间级输入和状态，必须由单个世界实例持有。
struct RoomState
{
    // 当前房间标识。
    int32_t roomId = 0;
    // 房间切换请求延迟到碰撞遍历结束，避免使遍历句柄失效。
    int32_t m_transferRoom = 0, m_transferToward = 1, m_stageId = 0, m_copyId = 0;
    FixedVec3 m_transferPosition;
    // 已清理房间不重复生成敌人。
    std::vector<int32_t> m_clearedRooms;
    // 是否暂停推进世界。
    bool paused = false;
    // 召唤接缝告警按场次去重。
    bool m_summonLogged = false;
    // 尚未移植的 Buff 配置标识按场次去重告警。
    std::vector<int32_t> m_unportedBuffIds;
    // 缺失消失动作的 Buff 配置标识按场次去重。
    std::vector<int32_t> m_missingBuffMotionIds;
    // 供场景逻辑消费的角色死亡事件，保留发生顺序。
    std::vector<Entity> m_roleDeaths;
    // 由帧输入收集、后续场景或角色流程消费的复活请求。
    std::vector<ReviveCommand> reviveRequests;
    // 由帧输入收集、后续角色流程消费的受控角色更新。
    std::vector<PuppetUpdate> puppetUpdates;
    // 是否启用房间流程及本场配置。
    bool m_enabled = false;
    conf::RoomConfig m_config;
    // 当前状态和去重的待切换队列。
    mugen::RoomState m_state = mugen::RoomState::None;
    std::vector<mugen::RoomState> m_pending, m_recorded;
    // 状态等待毫秒、战斗经过秒数和限时。
    Fixed m_waitMs, m_gameSeconds, m_limitSeconds;
    // 当前波次、定时器编号及到期标记。
    int32_t m_wave       = 0;
    uint64_t m_waveTimer = 0;
    bool m_waveReady     = false;
    // 副本等级增幅和复活预算。
    int32_t m_levelBonus = 0, m_maxRevives = 0, m_usedRevives = 0;
    // 首领、KO 和等待复活标记。
    bool m_haveBoss = false, m_ko = false, m_reviving = false;
    // 最终结果和返回城镇请求。
    bool m_victory = false, m_failure = false, m_returnToTown = false;
    // 参与世界快照的房间字段。
    MG_REFLECT(RoomState,
               roomId,
               m_transferRoom,
               m_transferToward,
               m_stageId,
               m_copyId,
               m_transferPosition,
               m_clearedRooms,
               paused,
               m_summonLogged,
               reviveRequests,
               puppetUpdates,
               m_roleDeaths,
               m_unportedBuffIds,
               m_missingBuffMotionIds,
               m_enabled,
               m_config,
               m_state,
               m_pending,
               m_recorded,
               m_waitMs,
               m_gameSeconds,
               m_limitSeconds,
               m_wave,
               m_waveTimer,
               m_waveReady,
               m_levelBonus,
               m_maxRevives,
               m_usedRevives,
               m_haveBoss,
               m_ko,
               m_reviving,
               m_victory,
               m_failure,
               m_returnToTown)
};
}  // namespace mugen::sim
// 生产世界允许保存的单例注册表，顺序固定类型标识。
#define MG_SINGLETON_LIST(X) \
    X(WorldClock)            \
    X(RngState) X(SlowMotion) X(LogicTimers) X(PresentationEvents) X(PlayerSlots) X(MapRegion) X(RoomState)
namespace mugen::sim
{
#define MG_SIM_SINGLETON_TUPLE(T) \
    , std::tuple<T> {}
// 由注册表展开的 ECS 单例元组。
using Singletons = decltype(std::tuple_cat(std::tuple<> {} MG_SINGLETON_LIST(MG_SIM_SINGLETON_TUPLE)));
#undef MG_SIM_SINGLETON_TUPLE
}  // namespace mugen::sim
