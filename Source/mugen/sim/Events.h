#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/core/reflect/Reflect.h"
#include <cstdint>

// 本文件登记全部请求事件（XxxRequest）与事实事件（XxxEvent）。
// 事件只存值与 Entity 句柄；请求由唯一所有者系统排空，事实只读，帧末由 FlushSystem 清空。
namespace mugen::sim
{
// 生成请求的实体种类，与 Identity.m_kind 对应。
enum class SpawnKind : uint8_t
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
// 销毁原因。
enum class DespawnReason : uint8_t
{
    // 动作片段结束。
    ClipEnd = 0,
    // 命中次数用尽。
    HitCount = 1,
    // 主人消失。
    OwnerGone = 2,
    // 被打断。
    Interrupted = 3,
    // 死亡动作结束。
    DeathEnd = 4
};
// 房间请求种类。
enum class RoomRequestKind : uint8_t
{
    // 复活。
    Revive = 0,
    // 回城。
    ReturnToTown = 1,
    // 警告确认。
    WarningDone = 2,
    // 传送。
    Transfer = 3
};
// 接触阶段。
enum class ContactPhase : uint8_t
{
    // 本帧开始接触。
    Began = 0,
    // 持续接触。
    Continue = 1,
    // 本帧结束接触。
    End = 2
};
// 命中结果。
enum class HitResult : uint8_t
{
    // 命中。
    Hit = 0,
    // 被闪避。
    Evade = 1,
    // 未命中。
    Miss = 2,
    // 命中但无伤害。
    NoDamage = 3
};
// 生命变化原因。
enum class HpCause : uint8_t
{
    // 受击伤害。
    Hit = 0,
    // 持续伤害。
    Dot = 1,
    // 治疗。
    Heal = 2,
    // 消耗。
    Cost = 3,
    // 护盾吸收。
    Shield = 4
};
// 施法事件阶段。
enum class CastPhase : uint8_t
{
    // 已准备。
    Prepared = 0,
    // 已开始。
    Began = 1,
    // 已结束。
    Ended = 2,
    // 已取消。
    Cancelled = 3,
    // 冷却开始。
    ColdStart = 4,
    // 冷却结束。
    ColdEnd = 5,
    // 能量耗尽。
    EpZero = 6,
    // 狂暴结束后。
    AfterCrazy = 7
};
// 动作事件阶段。
enum class ActionPhase : uint8_t
{
    // 动作开始。
    Begin = 0,
    // 动作结束。
    End = 1
};
// Buff 事件阶段。
enum class BuffPhase : uint8_t
{
    // 新增。
    Added = 0,
    // 叠层。
    Stacked = 1,
    // 移除。
    Removed = 2
};
// 提示事件种类（表现层据此选择音效、震屏等）。
enum class CueKind : uint8_t
{
    // 音效。
    Sound = 0,
    // 震屏。
    ShakeScreen = 1,
    // 残影。
    AfterImage = 2,
    // 提示文字。
    Tip = 3,
    // 对白。
    Dialogue = 4,
    // 全屏演出。
    FullScreen = 5,
    // 白框。
    WhiteFrame = 6,
    // 隐身视图。
    StealthView = 7,
    // 霸体描边。
    SuperArmorOutline = 8
};
// 房间事件种类。
enum class RoomEventKind : uint8_t
{
    // 房间状态变化。
    StateChanged = 0,
    // 胜利。
    Victory = 1,
    // 失败。
    Failure = 2,
    // 警告。
    Warning = 3,
    // PVP 结束。
    PvpEnded = 4
};
// 技能来源上下文，随生成请求传递给新实体。
struct SourceContext
{
    // 触发生成的技能配置标识。
    int32_t m_skillId = 0;
    // 基础技能标识（变化技能的原技能）。
    int32_t m_baseSkillId = 0;
    // 技能槽位。
    int32_t m_slot = 0;
    // 神器标识。
    int32_t m_artifactId = 0;
    // 神器序号。
    int32_t m_artifactIndex = 0;
    MG_REFLECT(SourceContext, m_skillId, m_baseSkillId, m_slot, m_artifactId, m_artifactIndex)
};

// ---------------- 请求事件 ----------------

// 请求在下一个同步点生成实体。
struct SpawnRequest
{
    // 实体种类。
    SpawnKind m_kind = SpawnKind::Role;
    // 配置标识。
    int32_t m_configId = 0;
    // 直接主人。
    Entity m_owner;
    // 来源上下文。
    SourceContext m_source;
    // 生成时已解析的位置。
    FixedVec3 m_position;
    // 朝向，仅取 -1 或 1。
    int32_t m_towardX = 1;
    // 初始水平速度向量。
    Fixed m_vectorX;
    // 初始纵深速度向量。
    Fixed m_vectorZ;
    // 寿命毫秒，0 表示由配置决定。
    int32_t m_lifetimeMs = 0;
    // 继承主人属性的位掩码。
    uint32_t m_inheritMask = 0;
    // 角色等级（角色出生时使用）。
    int32_t m_level = 1;
    // 阵营覆盖；0 表示使用角色配置默认阵营。
    int32_t m_camp = 0;
    // 玩家槽位；0 表示非玩家控制（配合 m_hasPlayerSlot 区分槽位 0）。
    uint32_t m_playerSlot = 0;
    // 是否有玩家槽位（槽位 0 也是有效槽位）。
    bool m_hasPlayerSlot = false;
    // 是否由 AI 驱动。
    bool m_isAi = false;
    // AI 难度等级。
    int32_t m_aiDifficulty = 1;
    MG_REFLECT(SpawnRequest,
               m_kind,
               m_configId,
               m_owner,
               m_source,
               m_position,
               m_towardX,
               m_vectorX,
               m_vectorZ,
               m_lifetimeMs,
               m_inheritMask,
               m_level,
               m_camp,
               m_playerSlot,
               m_hasPlayerSlot,
               m_isAi,
               m_aiDifficulty)
};
// 请求销毁实体（同时已立即打上 Despawning）。
struct DespawnRequest
{
    // 被销毁的实体。
    Entity m_entity;
    // 销毁原因。
    DespawnReason m_reason = DespawnReason::ClipEnd;
    MG_REFLECT(DespawnRequest, m_entity, m_reason)
};
// 请求角色变身。
struct TransformRequest
{
    // 变身的实体。
    Entity m_entity;
    // 目标角色配置标识。
    int32_t m_targetRoleId = 0;
    // 变身类型。
    int32_t m_type = 0;
    // 变身动作标识。
    int32_t m_actionId = 0;
    MG_REFLECT(TransformRequest, m_entity, m_targetRoleId, m_type, m_actionId)
};
// 请求判定实体死亡。
struct DeathRequest
{
    // 将死亡的实体。
    Entity m_entity;
    // 击杀者。
    Entity m_killer;
    // 造成击杀的特效。
    Entity m_effect;
    MG_REFLECT(DeathRequest, m_entity, m_killer, m_effect)
};
// 请求房间级流程。
struct RoomRequest
{
    // 请求种类。
    RoomRequestKind m_kind = RoomRequestKind::Revive;
    // 相关玩家槽位或目标实体的参数。
    int32_t m_param = 0;
    MG_REFLECT(RoomRequest, m_kind, m_param)
};

// ---------------- 事实事件 ----------------

// 实体已生成。
struct SpawnEvent
{
    // 新实体。
    Entity m_entity;
    // 实体种类。
    SpawnKind m_kind = SpawnKind::Role;
    // 配置标识。
    int32_t m_configId = 0;
    // 主人。
    Entity m_owner;
    // 语义键中的同配置序号。
    uint32_t m_ordinal = 0;
    MG_REFLECT(SpawnEvent, m_entity, m_kind, m_configId, m_owner, m_ordinal)
};
// 两个实体的接触。
struct ContactEvent
{
    // 接触方 a（攻击方）。
    Entity m_a;
    // 接触方 b（受击方）。
    Entity m_b;
    // 接触阶段。
    ContactPhase m_phase = ContactPhase::Began;
    // 接触点。
    FixedVec3 m_point;
    MG_REFLECT(ContactEvent, m_a, m_b, m_phase, m_point)
};
// 一次命中的结果。
struct HitEvent
{
    // 攻击者。
    Entity m_attacker;
    // 命中的特效。
    Entity m_effect;
    // 目标。
    Entity m_target;
    // 命中定义标识。
    int32_t m_hitDefId = 0;
    // 特效配置标识（供摘要 dmg 行使用）。
    int32_t m_effectId = 0;
    // 命中结果。
    HitResult m_result = HitResult::Hit;
    // 最终伤害。
    Fixed m_damage;
    // 是否暴击。
    bool m_critical = false;
    // 是否击杀。
    bool m_killed = false;
    // 接触点。
    FixedVec3 m_contact;
    MG_REFLECT(HitEvent, m_attacker, m_effect, m_target, m_hitDefId, m_effectId, m_result, m_damage, m_critical,
               m_killed, m_contact)
};
// 生命值变化。
struct HpEvent
{
    // 变化的实体。
    Entity m_entity;
    // 变化量，负为扣减。
    Fixed m_delta;
    // 变化原因。
    HpCause m_cause = HpCause::Hit;
    // 来源实体。
    Entity m_source;
    MG_REFLECT(HpEvent, m_entity, m_delta, m_cause, m_source)
};
// 实体死亡。
struct DeathEvent
{
    // 死亡的实体。
    Entity m_entity;
    // 击杀者。
    Entity m_killer;
    MG_REFLECT(DeathEvent, m_entity, m_killer)
};
// 施法阶段变化。
struct CastEvent
{
    // 施法实体。
    Entity m_entity;
    // 技能标识。
    int32_t m_skillId = 0;
    // 技能槽位下标（SkillBook::m_slots）。摘要导出再映射成字母。
    int32_t m_slot = 0;
    // 阶段。
    CastPhase m_phase = CastPhase::Prepared;
    MG_REFLECT(CastEvent, m_entity, m_skillId, m_slot, m_phase)
};
// 动作开始或结束。
struct ActionEvent
{
    // 动作所属实体。
    Entity m_entity;
    // 动作标识。
    int32_t m_actionId = 0;
    // 阶段。
    ActionPhase m_phase = ActionPhase::Begin;
    MG_REFLECT(ActionEvent, m_entity, m_actionId, m_phase)
};
// 角色状态转移。
struct StateEvent
{
    // 转移的实体。
    Entity m_entity;
    // 原状态编号。
    int32_t m_from = 0;
    // 新状态编号。
    int32_t m_to = 0;
    MG_REFLECT(StateEvent, m_entity, m_from, m_to)
};
// Buff 变化。
struct BuffEvent
{
    // Buff 所在实体。
    Entity m_entity;
    // Buff 配置标识。
    int32_t m_buffId = 0;
    // 实例序号。
    uint32_t m_serial = 0;
    // 阶段。
    BuffPhase m_phase = BuffPhase::Added;
    MG_REFLECT(BuffEvent, m_entity, m_buffId, m_serial, m_phase)
};
// 表现提示。
struct CueEvent
{
    // 提示相关实体。
    Entity m_entity;
    // 提示种类。
    CueKind m_kind = CueKind::Sound;
    // 主参数（资源或文本标识）。
    int32_t m_id = 0;
    // 附加参数。
    int32_t m_param = 0;
    MG_REFLECT(CueEvent, m_entity, m_kind, m_id, m_param)
};
// 房间流程事件。
struct RoomEvent
{
    // 事件种类。
    RoomEventKind m_kind = RoomEventKind::StateChanged;
    // 载荷（状态编号、胜利阵营等）。
    int32_t m_payload = 0;
    MG_REFLECT(RoomEvent, m_kind, m_payload)
};
// 实体已销毁。
struct DespawnEvent
{
    // 被销毁的实体。
    Entity m_entity;
    // 销毁原因。
    DespawnReason m_reason = DespawnReason::ClipEnd;
    MG_REFLECT(DespawnEvent, m_entity, m_reason)
};
// 位移事实的阶段。只由 displacement::rules 产生。
enum class DispPhase : int32_t
{
    // 曲线开始。
    Began = 0,
    // 曲线自然结束或被清空。
    Ended = 1,
    // 活动曲线被另一条曲线替换。
    Replaced = 2
};
// 位移事实。终点不入事件，由 DebugSink 在 P11 读 Transform。
struct DisplacementEvent
{
    // 位移所属实体。
    Entity m_entity;
    // 阶段。
    DispPhase m_phase = DispPhase::Began;
    // 曲线配置标识。
    int32_t m_curveId = 0;
    MG_REFLECT(DisplacementEvent, m_entity, m_phase, m_curveId)
};
// 顿帧事实。毫秒到帧的换算在 DebugSink。
struct HitStopEvent
{
    // 被顿帧的实体。
    Entity m_entity;
    // 写入的顿帧毫秒。
    Fixed m_freezeMs;
    MG_REFLECT(HitStopEvent, m_entity, m_freezeMs)
};
}  // namespace mugen::sim
