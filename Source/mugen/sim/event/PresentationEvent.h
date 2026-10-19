#pragma once
#include "mugen/core/ecs/Entity.h"

namespace mugen::sim
{
// 请求表现层播放或停止音效。
struct Sound
{
    // 音效配置标识。
    int32_t id = 0;
    // 可供表现层随机选择的候选音效。
    std::vector<int32_t> m_candidates;
    // 是否停止循环音效。
    bool m_stopLoop = false;
    // 关联的动作标识。
    int32_t m_actionId = -1;
    // 参与事件快照的音效字段。
    MG_REFLECT(Sound, id, m_candidates, m_stopLoop, m_actionId)
};
// 请求表现层执行镜头震动。
struct CameraShake
{
    // 震动强度。
    Fixed strength;
    // 震动持续毫秒数。
    Fixed durationMs;
    // 目标镜头标识。
    int32_t m_cameraId = -1;
    // 参与事件快照的镜头字段。
    MG_REFLECT(CameraShake, strength, durationMs, m_cameraId)
};
// 请求表现层展示骨骼特效。
struct DisplaySpine
{
    // 特效配置标识。
    int32_t id = 0;
    // 特效分组参数。
    std::vector<std::vector<int32_t>> m_groups;
    // 参与事件快照的特效字段。
    MG_REFLECT(DisplaySpine, id, m_groups)
};
// 请求表现层显示伤害数值。
struct HurtNumber
{
    // 显示的伤害或治疗数值。
    int64_t value = 0;
    // 参与事件快照的数值字段。
    MG_REFLECT(HurtNumber, value)
};
// 请求表现层保留残影。
struct Ghost
{
    // 残影持续毫秒数。
    Fixed durationMs;
    // 参与事件快照的残影字段。
    MG_REFLECT(Ghost, durationMs)
};
// 请求表现层显示气泡。
struct Bubble
{
    // 气泡配置标识。
    int32_t id = 0;
    // 参与事件快照的气泡字段。
    MG_REFLECT(Bubble, id)
};
// 请求表现层显示提示文本。
struct Tips
{
    // 待显示的文本内容。
    std::string text;
    // 参与事件快照的提示字段。
    MG_REFLECT(Tips, text)
};
// 通知表现层房间状态发生变化。
struct RoomStateChanged
{
    // 新的房间状态值。
    int32_t state = 0;
    // 参与事件快照的房间状态字段。
    MG_REFLECT(RoomStateChanged, state)
};
// 通知表现层房间胜利。
struct Victory
{
    // 产生结果的房间标识。
    int32_t roomId = 0;
    // 参与事件快照的胜利字段。
    MG_REFLECT(Victory, roomId)
};
// 通知表现层房间失败。
struct Failure
{
    // 产生结果的房间标识。
    int32_t roomId = 0;
    // 参与事件快照的失败字段。
    MG_REFLECT(Failure, roomId)
};
// 角色死亡完成后发出的表现通知。
struct RoleDeath
{
    // 死亡角色的配置标识。
    int32_t m_roleId = 0;
    MG_REFLECT(RoleDeath, m_roleId)
};
// 请求表现层改变角色受击亮度。
struct RoleIntensity
{
    // 亮度倍率，正常显示为一。
    Fixed m_value = "1"_fx;
    MG_REFLECT(RoleIntensity, m_value)
};
// 请求表现层显示或隐藏白帧保护效果。
struct WhiteFrame
{
    // 是否显示保护效果。
    bool m_enabled = false;
    MG_REFLECT(WhiteFrame, m_enabled)
};
// 所有可序列化的表现事件载荷类型。
// Buff 骨骼阶段事件，不允许表现完成回调改变模拟状态。
struct BuffSpine
{
    // 实例序号、骨骼配置和阶段：一出现、二持续、三消失、零删除。
    uint64_t m_serial = 0;
    int32_t m_spineId = -1, m_stage = 0;
    // 配置来源供表现层读取偏移、层级、朝向及完整阶段标记。
    int32_t m_buffId = -1;
    MG_REFLECT(BuffSpine, m_serial, m_spineId, m_stage, m_buffId)
};
// 房间警告配置，首领警告等待输入确认。
struct RoomWarning
{
    // 警告资源编号。
    int32_t m_id = 0;
    // 是否暂停等待确认。
    bool m_boss = false;
    MG_REFLECT(RoomWarning, m_id, m_boss)
};
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
                                         BuffSpine,
                                         RoomWarning>;
// 由逻辑帧产生、仅供表现层消费的事件。
struct PresentationEvent
{
    // 产生事件的逻辑帧。
    uint64_t frame = 0;
    // 事件所属实体。
    Entity entity;
    // 同一帧内的稳定顺序号。
    uint64_t seq = 0;
    // 事件具体内容。
    PresentationPayload payload;
    // 参与事件快照的事件字段。
    MG_REFLECT(PresentationEvent, frame, entity, seq, payload)
};
// 每场世界独有的表现事件队列。
struct PresentationEvents
{
    // 等待表现层消费的事件。
    std::vector<PresentationEvent> events;
    // 下一个事件使用的稳定顺序号。
    uint64_t nextSeq = 0;
    // 参与世界快照的队列字段。
    MG_REFLECT(PresentationEvents, events, nextSeq)
};
}  // namespace mugen::sim
