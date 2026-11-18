#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/core/reflect/Reflect.h"
#include <cstdint>
#include <vector>

// 本文件登记全部请求事件（XxxRequest）与事实事件（XxxEvent）。
// 事件只存值与 Entity 句柄；请求由唯一所有者系统排空，事实只读，帧末由 FlushSystem 清空。
namespace mugen::sim
{
// 生成请求的实体种类，与 IdentityComponent.kind 对应。
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
    SuperArmorOutline = 8,
    // 墨汁屏。
    InkScreen = 9,
    // 攻击变身切换显示。
    TransformSwap = 10,
    // 伙伴怒气变身。
    PartnerRage = 11
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
    int32_t skillId = 0;
    // 基础技能标识（变化技能的原技能）。
    int32_t baseSkillId = 0;
    // 技能槽位。
    int32_t slot = 0;
    // 神器标识。
    int32_t artifactId = 0;
    // 神器序号。
    int32_t artifactIndex = 0;
    MG_REFLECT(SourceContext, skillId, baseSkillId, slot, artifactId, artifactIndex)
};

// ---------------- 请求事件 ----------------

// 请求在下一个同步点生成实体。
struct SpawnRequest
{
    // 实体种类。
    SpawnKind kind = SpawnKind::Role;
    // 配置标识。
    int32_t configId = 0;
    // 直接主人。
    Entity owner;
    // 来源上下文。
    SourceContext source;
    // 生成时已解析的位置。
    FixedVec3 position;
    // 朝向，仅取 -1 或 1。
    int32_t towardX = 1;
    // 初始水平速度向量。
    Fixed vectorX;
    // 初始纵深速度向量。
    Fixed vectorZ;
    // 寿命毫秒，0 表示由配置决定。
    int32_t lifetimeMs = 0;
    // 继承主人属性的位掩码。
    uint32_t inheritMask = 0;
    // 角色等级（角色出生时使用）。
    int32_t level = 1;
    // 阵营覆盖；0 表示使用角色配置默认阵营。
    int32_t camp = 0;
    // 玩家槽位；0 表示非玩家控制（配合 hasPlayerSlot 区分槽位 0）。
    uint32_t playerSlot = 0;
    // 是否有玩家槽位（槽位 0 也是有效槽位）。
    bool hasPlayerSlot = false;
    // 是否由 AI 驱动。
    bool isAi = false;
    // AI 难度等级。
    int32_t aiDifficulty = 1;
    // 受击特效要记住的目标。只有没有后继的特效才写入身份。
    Entity hitEntity;
    MG_REFLECT(SpawnRequest,
               kind,
               configId,
               owner,
               source,
               position,
               towardX,
               vectorX,
               vectorZ,
               lifetimeMs,
               inheritMask,
               level,
               camp,
               playerSlot,
               hasPlayerSlot,
               isAi,
               aiDifficulty,
               hitEntity)
};
// 请求销毁实体（同时已立即打上 DespawningComponent）。
struct DespawnRequest
{
    // 被销毁的实体。
    Entity entity;
    // 销毁原因。
    DespawnReason reason = DespawnReason::ClipEnd;
    MG_REFLECT(DespawnRequest, entity, reason)
};
// 请求角色变身。
struct TransformRequest
{
    // 变身的实体。
    Entity entity;
    // 目标角色配置标识。
    int32_t targetRoleId = 0;
    // 变身类型。
    int32_t type = 0;
    // 变身动作标识。
    int32_t actionId = 0;
    MG_REFLECT(TransformRequest, entity, targetRoleId, type, actionId)
};
// 请求判定实体死亡。
struct DeathRequest
{
    // 将死亡的实体。
    Entity entity;
    // 击杀者。
    Entity killer;
    // 造成击杀的特效。
    Entity effect;
    MG_REFLECT(DeathRequest, entity, killer, effect)
};
// 请求房间级流程。
struct RoomRequest
{
    // 请求种类。
    RoomRequestKind kind = RoomRequestKind::Revive;
    // 相关玩家槽位或目标实体的参数。
    int32_t param = 0;
    MG_REFLECT(RoomRequest, kind, param)
};

// ---------------- 事实事件 ----------------

// 实体已生成。
struct SpawnEvent
{
    // 新实体。
    Entity entity;
    // 实体种类。
    SpawnKind kind = SpawnKind::Role;
    // 配置标识。
    int32_t configId = 0;
    // 主人。
    Entity owner;
    // 语义键中的同配置序号。
    uint32_t ordinal = 0;
    MG_REFLECT(SpawnEvent, entity, kind, configId, owner, ordinal)
};
// 两个实体的接触。
struct ContactEvent
{
    // 接触方 a（攻击方）。
    Entity a;
    // 接触方 b（受击方）。
    Entity b;
    // 接触阶段。
    ContactPhase phase = ContactPhase::Began;
    // 接触点。
    FixedVec3 point;
    MG_REFLECT(ContactEvent, a, b, phase, point)
};
// 一次命中的结果。
struct HitEvent
{
    // 攻击者。
    Entity attacker;
    // 命中的特效。
    Entity effect;
    // 目标。
    Entity target;
    // 命中定义标识。
    int32_t hitDefId = 0;
    // 特效配置标识（供摘要 dmg 行使用）。
    int32_t effectId = 0;
    // 命中结果。
    HitResult result = HitResult::Hit;
    // 最终伤害。
    Fixed damage;
    // 是否暴击。
    bool critical = false;
    // 是否击杀。
    bool killed = false;
    // 接触点。
    FixedVec3 contact;
    MG_REFLECT(HitEvent, attacker, effect, target, hitDefId, effectId, result, damage, critical,
               killed, contact)
};
// 生命值变化。
struct HpEvent
{
    // 变化的实体。
    Entity entity;
    // 变化量，负为扣减。
    Fixed delta;
    // 变化原因。
    HpCause cause = HpCause::Hit;
    // 来源实体。
    Entity source;
    MG_REFLECT(HpEvent, entity, delta, cause, source)
};
// 实体死亡。
struct DeathEvent
{
    // 死亡的实体。
    Entity entity;
    // 击杀者。
    Entity killer;
    MG_REFLECT(DeathEvent, entity, killer)
};
// 施法阶段变化。
struct CastEvent
{
    // 施法实体。
    Entity entity;
    // 技能标识。
    int32_t skillId = 0;
    // 技能槽位下标（SkillBookComponent::slots）。摘要导出再映射成字母。
    int32_t slot = 0;
    // 阶段。
    CastPhase phase = CastPhase::Prepared;
    MG_REFLECT(CastEvent, entity, skillId, slot, phase)
};
// 动作开始或结束。
struct ActionEvent
{
    // 动作所属实体。
    Entity entity;
    // 动作标识。
    int32_t actionId = 0;
    // 阶段。
    ActionPhase phase = ActionPhase::Begin;
    MG_REFLECT(ActionEvent, entity, actionId, phase)
};
// 角色状态转移。
struct StateEvent
{
    // 转移的实体。
    Entity entity;
    // 原状态编号。
    int32_t from = 0;
    // 新状态编号。
    int32_t to = 0;
    MG_REFLECT(StateEvent, entity, from, to)
};
// Buff 变化。
struct BuffEvent
{
    // Buff 所在实体。
    Entity entity;
    // Buff 配置标识。
    int32_t buffId = 0;
    // 实例序号。
    uint32_t serial = 0;
    // 阶段。
    BuffPhase phase = BuffPhase::Added;
    MG_REFLECT(BuffEvent, entity, buffId, serial, phase)
};
// 表现提示。
struct CueEvent
{
    // 提示相关实体。
    Entity entity;
    // 提示种类。
    CueKind kind = CueKind::Sound;
    // 主参数（资源或文本标识）。
    int32_t id = 0;
    // 附加参数（动作标识、开关值、变身目标句柄等）。
    int32_t param = 0;
    // 是否停止循环音效。
    bool stopLoop = false;
    // 供表现层自行挑选的候选音效。
    std::vector<int32_t> candidates;
    // 全屏骨骼分组。
    std::vector<std::vector<int32_t>> groups;
    MG_REFLECT(CueEvent, entity, kind, id, param, stopLoop, candidates, groups)
};
// 房间流程事件。
struct RoomEvent
{
    // 事件种类。
    RoomEventKind kind = RoomEventKind::StateChanged;
    // 载荷：状态变化时为房间状态编号；胜利/失败时为房间标识；警告时为警告编号。
    int32_t payload = 0;
    // 附加参数：警告事件时为是否暂停推进（1 暂停）。
    int32_t param = 0;
    // 相关实体（英雄死亡进入复活/失败时指向该英雄）。
    Entity entity;
    MG_REFLECT(RoomEvent, kind, payload, param, entity)
};
// 实体已销毁。
struct DespawnEvent
{
    // 被销毁的实体。
    Entity entity;
    // 销毁原因。
    DespawnReason reason = DespawnReason::ClipEnd;
    // 实体种类（销毁后组件不可读，预先记下）。
    SpawnKind kind = SpawnKind::Role;
    // 配置标识。
    int32_t configId = 0;
    // 销毁时仍在播的动作片段标识。
    int32_t clipId = 0;
    MG_REFLECT(DespawnEvent, entity, reason, kind, configId, clipId)
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
// 位移事实。终点不入事件，由 DebugSinkSystem 在 P11 读 TransformComponent。
struct DisplacementEvent
{
    // 位移所属实体。
    Entity entity;
    // 阶段。
    DispPhase phase = DispPhase::Began;
    // 曲线配置标识。
    int32_t curveId = 0;
    MG_REFLECT(DisplacementEvent, entity, phase, curveId)
};
// 顿帧事实。毫秒到帧的换算在 DebugSinkSystem。
struct HitStopEvent
{
    // 被顿帧的实体。
    Entity entity;
    // 写入的顿帧毫秒。
    Fixed freezeMs;
    MG_REFLECT(HitStopEvent, entity, freezeMs)
};
}  // namespace mugen::sim
