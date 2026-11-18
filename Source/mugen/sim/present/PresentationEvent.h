#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/FixedVec2.h"
#include "mugen/core/reflect/Reflect.h"
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace mugen::sim
{
// 表现层看到的实体种类。数值与生成种类对齐，避免本头依赖事实事件定义。
enum class PresentEntityKind : uint8_t
{
    // 角色。
    Role = 0,
    // 特效。
    Effect = 1,
    // 障碍。
    Obstacle = 2,
    // 物品。
    Goods = 3,
    // 传送门。
    Portal = 4
};

// 请求表现层播放或停止音效。
struct Sound
{
    // 音效配置标识。
    int32_t id = 0;
    // 供表现层自行挑选的候选音效。
    std::vector<int32_t> candidates;
    // 是否停止循环音效。
    bool stopLoop = false;
    // 关联的动作标识。
    int32_t actionId = -1;
    MG_REFLECT(Sound, id, candidates, stopLoop, actionId)
};

// 请求表现层执行镜头震动。
struct CameraShake
{
    // 震动强度。
    Fixed strength;
    // 震动持续毫秒。
    Fixed durationMs;
    // 目标镜头标识。
    int32_t cameraId = -1;
    MG_REFLECT(CameraShake, strength, durationMs, cameraId)
};

// 请求表现层展示骨骼特效。
struct DisplaySpine
{
    // 特效配置标识。
    int32_t id = 0;
    // 特效分组参数。
    std::vector<std::vector<int32_t>> groups;
    MG_REFLECT(DisplaySpine, id, groups)
};

// 请求表现层显示伤害或治疗数值。
struct HurtNumber
{
    // 显示的数值；正为伤害，负为治疗。
    int64_t value = 0;
    // 本次伤害是否暴击。
    bool critical = false;
    // 是否携带命中接触点。
    bool hasContact = false;
    // 命中接触点（屏幕平面：横向与高度加纵深）。
    FixedVec2 contact;
    MG_REFLECT(HurtNumber, value, critical, hasContact, contact)
};

// 请求表现层保留残影。
struct Ghost
{
    // 残影持续毫秒；负一表示开始，零表示停止。
    Fixed durationMs;
    MG_REFLECT(Ghost, durationMs)
};

// 请求表现层显示气泡。
struct Bubble
{
    // 气泡配置标识。
    int32_t id = 0;
    MG_REFLECT(Bubble, id)
};

// 请求表现层显示提示文本。
struct Tips
{
    // 待显示的文本内容。
    std::string text;
    MG_REFLECT(Tips, text)
};

// 通知表现层房间状态发生变化。
struct RoomStateChanged
{
    // 新的房间状态值。
    int32_t state = 0;
    MG_REFLECT(RoomStateChanged, state)
};

// 通知表现层房间胜利。
struct Victory
{
    // 产生结果的房间标识。
    int32_t roomId = 0;
    MG_REFLECT(Victory, roomId)
};

// 通知表现层房间失败。
struct Failure
{
    // 产生结果的房间标识。
    int32_t roomId = 0;
    MG_REFLECT(Failure, roomId)
};

// 角色死亡动作完成后的表现通知。
struct RoleDeath
{
    // 死亡角色的配置标识。
    int32_t roleId = 0;
    MG_REFLECT(RoleDeath, roleId)
};

// 请求表现层改变角色受击亮度。
struct RoleIntensity
{
    // 亮度倍率，正常显示为一。
    Fixed value = "1"_fx;
    MG_REFLECT(RoleIntensity, value)
};

// 请求表现层显示或隐藏白帧保护。
struct WhiteFrame
{
    // 是否显示保护效果。
    bool enabled = false;
    MG_REFLECT(WhiteFrame, enabled)
};

// 通知表现层伙伴怒气变身状态切换。
struct PartnerRageChanged
{
    // 是否进入变身态。
    bool active = false;
    MG_REFLECT(PartnerRageChanged, active)
};

// Buff 骨骼阶段事件：一出现、二持续、三消失、零删除。
struct BuffSpine
{
    // 实例序号。
    uint64_t serial = 0;
    // 骨骼资源标识。
    int32_t spineId = -1;
    // 播放阶段。
    int32_t stage = 0;
    // 配置来源，供表现层读偏移与层级。
    int32_t buffId = -1;
    MG_REFLECT(BuffSpine, serial, spineId, stage, buffId)
};

// 房间警告；首领警告等待输入确认。
struct RoomWarning
{
    // 警告资源编号。
    int32_t id = 0;
    // 是否暂停等待确认。
    bool boss = false;
    MG_REFLECT(RoomWarning, id, boss)
};

// 决斗回合结束原因。
enum class PvpRoundEndReason : int32_t
{
    // 一方阵营被击倒清空。
    Ko = 0,
    // 决斗限时到达。
    Timeout = 1
};

// 决斗回合结束通知。
struct PvpRoundEnd
{
    // 获胜阵营位；零表示平局。
    int32_t winnerCamp = 0;
    // 结束原因。
    PvpRoundEndReason reason = PvpRoundEndReason::Ko;
    // 各阵营剩余生命比例。
    std::map<int32_t, Fixed> hpRatios;
    MG_REFLECT(PvpRoundEnd, winnerCamp, reason, hpRatios)
};

// 实体已生成。
struct EntitySpawned
{
    // 实体种类。
    PresentEntityKind kind = PresentEntityKind::Role;
    // 配置标识。
    int32_t configId = 0;
    MG_REFLECT(EntitySpawned, kind, configId)
};

// 实体已销毁。
struct EntityDespawned
{
    // 实体种类。
    PresentEntityKind kind = PresentEntityKind::Role;
    // 配置标识。
    int32_t configId = 0;
    MG_REFLECT(EntityDespawned, kind, configId)
};

// 变身时旧实体与新实体的显示切换。
struct TransformSwap
{
    // 旧实体。
    Entity oldEntity;
    // 新实体。
    Entity newEntity;
    MG_REFLECT(TransformSwap, oldEntity, newEntity)
};

// 请求表现层切换隐身视图。
struct StealthView
{
    // 是否启用隐身视图。
    bool enabled = false;
    MG_REFLECT(StealthView, enabled)
};

// 请求表现层显示或隐藏霸体描边。
struct SuperArmorBorder
{
    // 是否显示描边。
    bool enabled = false;
    MG_REFLECT(SuperArmorBorder, enabled)
};

// 可序列化的表现事件载荷。
using PresentationPayload = std::variant<Sound,
                                         CameraShake,
                                         DisplaySpine,
                                         HurtNumber,
                                         Ghost,
                                         Bubble,
                                         Tips,
                                         RoomStateChanged,
                                         Victory,
                                         Failure,
                                         RoleDeath,
                                         RoleIntensity,
                                         WhiteFrame,
                                         PartnerRageChanged,
                                         BuffSpine,
                                         RoomWarning,
                                         PvpRoundEnd,
                                         EntitySpawned,
                                         EntityDespawned,
                                         TransformSwap,
                                         StealthView,
                                         SuperArmorBorder>;

// 由逻辑帧产生、仅供表现层消费的事件。
struct PresentationEvent
{
    // 产生事件的逻辑帧。
    uint64_t frame = 0;
    // 事件所属实体。
    Entity entity;
    // 同一帧内的稳定顺序号，每帧从零重计。
    uint64_t ordinal = 0;
    // 全局单调顺序号。
    uint64_t seq = 0;
    // 事件具体内容。
    PresentationPayload payload;
    MG_REFLECT(PresentationEvent, frame, entity, ordinal, seq, payload)
};

// 每场世界独有的表现事件队列。
struct PresentationEvents
{
    // 等待表现层消费的事件。
    std::vector<PresentationEvent> events;
    // 下一个事件使用的全局顺序号。
    uint64_t nextSeq = 0;
    MG_REFLECT(PresentationEvents, events, nextSeq)
};
}  // namespace mugen::sim
