#pragma once
#include "Events.h"
#include "mugen/sim/present/PresentationEvent.h"
#include "WorldSetup.h"
#include "mugen/sim/motion/MapRegion.h"
#include "mugen/sim/room/KoSlowMotion.h"
#include "mugen/core/ecs/EventQueue.h"
#include "mugen/core/math/Random.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/conf/tables/MapTables.h"
#include "mugen/conf/GameDef.h"
#include <map>
#include <tuple>

namespace mugen::sim
{
// 每场世界独有的固定帧时钟。
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
    MG_REFLECT(WorldClock, frame, elapsedMs, dtSec, dtMs)
};
// 每场世界独有的确定性随机数状态；逻辑代码只经它抽样。
struct RngState
{
    // 本场唯一的随机数发生器。
    Random random;
    MG_REFLECT(RngState, random)
};
// 当前世界的慢动作控制状态：外部定长慢放 + 房间 KO 曲线。
struct SlowMotion
{
    // 应用于逻辑时间的缩放倍率。
    Fixed scale = Fixed::fromInt(1);
    // 仍需保持缩放的逻辑帧数。
    uint32_t remainingFrames = 0;
    // 房间 KO 曲线独立于外部指定的定长慢放。
    room::KoSlowMotion ko;
    MG_REFLECT(SlowMotion, scale, remainingFrames, ko)
};
// 玩家槽位到角色实体的确定性映射。
struct PlayerSlots
{
    // 按玩家槽位保存其控制的实体句柄。
    std::map<uint32_t, Entity> entities;
    MG_REFLECT(PlayerSlots, entities)
};
// 房间级状态（含房间状态机、波次、复活与胜负结算）。
struct RoomState
{
    // 当前房间标识。
    int32_t roomId = 0;
    // 是否启用房间流程。
    bool enabled = false;
    // 是否暂停推进世界。
    bool paused = false;
    // 副本等级增幅（怪物等级加成）。
    int32_t levelBonus = 0;
    // 是否已触发 KO（首波怪物清空后进入慢放结算）。
    bool ko = false;
    // 是否已检测到 Boss 清空、等待下一帧真正启动 KO 慢放（对齐旧实现「房间导演先于碰撞」的一帧延迟）。
    bool pendingKo = false;
    // 房间是否包含 Boss（roleType 4）。
    bool haveBoss = false;
    // 房间类型（0 表示无 KO 慢放）。
    int32_t roomType = 0;
    // 战斗规则类型（< 2 时才触发 KO 慢放）。
    int32_t battleRuleType = 0;
    // 本场房间配置（波次、限时、掉落率等运行时读取）。
    conf::RoomConfig config;
    // 当前状态与去重的待切换队列。
    mugen::RoomState state = mugen::RoomState::None;
    std::vector<mugen::RoomState> pending;
    std::vector<mugen::RoomState> recorded;
    // 状态等待毫秒、战斗经过秒数与限时。
    Fixed waitMs;
    Fixed gameSeconds;
    Fixed limitSeconds;
    // 当前波次下标、波次倒计时毫秒与到期标记。
    int32_t wave = 0;
    Fixed waveTimerMs;
    bool waveReady = false;
    // 复活预算。
    int32_t maxRevives = 0;
    int32_t usedRevives = 0;
    // 等待复活标记。
    bool reviving = false;
    // 最终结果与返回城镇请求。
    bool victory = false;
    bool failure = false;
    bool returnToTown = false;
    // 已清理房间不重复生成敌人。
    std::vector<int32_t> clearedRooms;
    // 供场景逻辑消费的角色死亡事件，保留发生顺序。
    std::vector<Entity> roleDeaths;
    // 房间切换请求延迟到帧末，避免使遍历句柄失效。
    int32_t transferRoom = 0;
    int32_t transferToward = 1;
    FixedVec3 transferPosition;
    // 进入房间时使用的关卡与副本标识。
    int32_t stageId = 0;
    int32_t copyId = 0;
    // 是否为 PVP 决斗（进入房间时由 WorldSetup 写入；本切片仅保留字段）。
    bool pvp = false;
    Fixed pvpTimeLimit;
    int32_t winnerCamp = 0;
    MG_REFLECT(RoomState, roomId, enabled, paused, levelBonus, ko, pendingKo, haveBoss, roomType,
               battleRuleType, config, state, pending, recorded, waitMs, gameSeconds, limitSeconds,
               wave, waveTimerMs, waveReady, maxRevives, usedRevives, reviving, victory, failure,
               returnToTown, clearedRooms, roleDeaths, transferRoom, transferToward, transferPosition,
               stageId, copyId, pvp, pvpTimeLimit, winnerCamp)
};
// 生成登记表：各配置的 ordinal 计数（语义键 role:<configId>#<n>）。
struct SpawnRegistry
{
    // 配置标识 -> 已生成数量。
    std::map<int32_t, uint32_t> ordinalCounts;
    // 取并递增指定配置的下一个 ordinal（从 1 起）。
    uint32_t next(int32_t configId)
    {
        return ++ordinalCounts[configId];
    }
    MG_REFLECT(SpawnRegistry, ordinalCounts)
};
// 一条延迟动作（B2 仅顿帧延迟）。
struct ScheduledAction
{
    // 唯一标识。
    uint64_t id = 0;
    // 所属实体（销毁时一并取消）。
    Entity owner;
    // 到期剩余毫秒。
    Fixed dueMs;
    // 登记序号（到期时间相同时按序号稳定排序）。
    uint64_t seq = 0;
    // 顿帧延迟：到期时写入受击者的顿帧时长。
    Fixed freezeMs;
    MG_REFLECT(ScheduledAction, id, owner, dueMs, seq, freezeMs)
};
// 有序延迟动作表（取代旧 LogicTimers 的受击顿帧延迟）。
struct ScheduledActions
{
    // 按登记顺序保存的活动动作。
    std::vector<ScheduledAction> actions;
    // 下一个动作标识。
    uint64_t nextId = 1;
    // 下一个登记序号。
    uint64_t nextSeq = 0;
    // 登记顿帧延迟并返回标识；到期后写受击者顿帧。
    uint64_t add(Entity owner, Fixed dueMs, Fixed freezeMs)
    {
        actions.push_back({nextId, owner, dueMs, nextSeq++, freezeMs});
        return nextId++;
    }
    // 按标识取消（新命中取消旧的待生效顿帧）。
    void cancel(uint64_t id)
    {
        std::erase_if(actions, [id](const ScheduledAction& a) { return a.id == id; });
    }
    // 取消指定实体的全部动作。
    void cancelOwner(Entity owner)
    {
        std::erase_if(actions, [owner](const ScheduledAction& a) { return a.owner == owner; });
    }
    MG_REFLECT(ScheduledActions, actions, nextId, nextSeq)
};
}  // namespace mugen::sim

// 新 sim 单例唯一登记表：时钟、随机数、全部事件队列、表现缓冲、地图、槽位、房间与生成登记。
// clang-format off
#define MG_SINGLETON_LIST(X)         \
    X(WorldClock)                    \
    X(RngState)                      \
    X(SlowMotion)                    \
    X(PresentationEvents)            \
    X(PlayerSlots)                   \
    X(MapRegion)                     \
    X(RoomState)                     \
    X(SpawnRegistry)                 \
    X(ScheduledActions)              \
    X(EventQueue<SpawnRequest>)      \
    X(EventQueue<DespawnRequest>)    \
    X(EventQueue<TransformRequest>)  \
    X(EventQueue<DeathRequest>)      \
    X(EventQueue<RoomRequest>)       \
    X(EventQueue<SpawnEvent>)        \
    X(EventQueue<ContactEvent>)      \
    X(EventQueue<HitEvent>)          \
    X(EventQueue<HpEvent>)           \
    X(EventQueue<DeathEvent>)        \
    X(EventQueue<CastEvent>)         \
    X(EventQueue<ActionEvent>)       \
    X(EventQueue<StateEvent>)        \
    X(EventQueue<BuffEvent>)         \
    X(EventQueue<CueEvent>)          \
    X(EventQueue<RoomEvent>)         \
    X(EventQueue<DespawnEvent>)      \
    X(EventQueue<DisplacementEvent>) \
    X(EventQueue<HitStopEvent>)
// clang-format on

namespace mugen::sim
{
#define MG_SIM_SINGLETON_TUPLE(T) \
    , std::tuple<T> {}
// 由登记表展开的 ECS 单例元组。
using Singletons = decltype(std::tuple_cat(std::tuple<> {} MG_SINGLETON_LIST(MG_SIM_SINGLETON_TUPLE)));
#undef MG_SIM_SINGLETON_TUPLE
}  // namespace mugen::sim
