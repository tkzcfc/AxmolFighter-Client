#pragma once
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/core/reflect/Reflect.h"
#include <algorithm>

namespace mugen::sim
{
// 角色静态身份和阵营信息。
struct RoleIdentity
{
    // 角色配置标识。
    int32_t roleId = 0;
    // 角色类型标识。
    int32_t roleType = 1;
    // 角色等级。
    int32_t level = 1;
    // AI 配置标识。
    int32_t aiId = 0;
    // AI 难度等级。
    int32_t aiDifficulty = 1;
    // 所属阵营位。
    int32_t camp = 2;
    // 可敌对阵营的位掩码。
    int32_t hostileCamp = 25;
    // 是否由 AI 驱动。
    bool isAi = false;
    // 可选的控制玩家槽位。
    std::optional<uint32_t> playerSlot;
    // 召唤角色的归属句柄，召唤系统负责赋值。
    Entity m_owner;
    // 参与组件快照的身份字段。
    MG_REFLECT(RoleIdentity, roleId, roleType, level, aiId, aiDifficulty, camp, hostileCamp, isAi, playerSlot, m_owner)
};
// 状态切换时附带的参数。
struct EntityParams
{
    // 当前受击配置标识。
    std::optional<int32_t> hit_id;
    // 连续受击次数。
    int32_t hit_counts = 0;
    // 移动目标位置。
    FixedVec3 move_pos;
    // 寻路剩余路径点按行进顺序保存，空列表使用单目标位置。
    std::vector<FixedVec3> m_path;
    // 当前位移配置标识。
    int32_t displacement_id = 0;
    // 死亡位移配置标识。
    int32_t death_displacement_id = 0;
    // 参与组件快照的状态参数字段。
    MG_REFLECT(EntityParams, hit_id, hit_counts, move_pos, m_path, displacement_id, death_displacement_id)
};
// 角色状态位及其附加参数。
struct RoleStatus
{
    // 当前可组合状态位。
    EntityRoleStatus eEntityStatus = EntityRoleStatus::Idle;
    // 状态转换携带的参数。
    EntityParams entityParams;
    // 检查请求的全部状态位是否同时存在。
    bool has(EntityRoleStatus bits) const
    {
        return (static_cast<uint32_t>(eEntityStatus) & static_cast<uint32_t>(bits)) == static_cast<uint32_t>(bits);
    }
    // 将状态位并入角色状态。
    void mix(EntityRoleStatus bits) { eEntityStatus = eEntityStatus | bits; }
    // 仅在已存在时清除状态位。
    void abort(EntityRoleStatus bits)
    {
        if (has(bits))
            eEntityStatus = static_cast<EntityRoleStatus>(static_cast<uint32_t>(eEntityStatus) ^
                                                          static_cast<uint32_t>(bits));
    }
    // 为每个状态位生成只读查询函数。
#define MG_ROLE_STATUS_QUERY(Name) \
    bool is##Name() const { return has(EntityRoleStatus::Name); }
    MG_ROLE_STATUS_QUERY(None)
    MG_ROLE_STATUS_QUERY(Birth)
    MG_ROLE_STATUS_QUERY(Death)
    MG_ROLE_STATUS_QUERY(Wake)
    MG_ROLE_STATUS_QUERY(GetUp)
    MG_ROLE_STATUS_QUERY(HitFloor)
    MG_ROLE_STATUS_QUERY(HitDown)
    MG_ROLE_STATUS_QUERY(HitUp)
    MG_ROLE_STATUS_QUERY(HitSwitch)
    MG_ROLE_STATUS_QUERY(Hit)
    MG_ROLE_STATUS_QUERY(Attack)
    MG_ROLE_STATUS_QUERY(Fall)
    MG_ROLE_STATUS_QUERY(Patrol)
    MG_ROLE_STATUS_QUERY(Chase)
    MG_ROLE_STATUS_QUERY(Alert)
    MG_ROLE_STATUS_QUERY(Run)
    MG_ROLE_STATUS_QUERY(Walk)
    MG_ROLE_STATUS_QUERY(Idle)
    MG_ROLE_STATUS_QUERY(Finding)
    MG_ROLE_STATUS_QUERY(Jostled)
    MG_ROLE_STATUS_QUERY(Revive)
    MG_ROLE_STATUS_QUERY(Controlled)
#undef MG_ROLE_STATUS_QUERY
    // 判断角色是否处于可被打断的受击状态。
    bool isCanBreak() const { return isHitFloor() || isHitDown() || isHitUp() || isHitSwitch() || isHit(); }
    // 判断角色是否处于可受击状态。
    bool isTobeHit() const { return isGetUp() || isCanBreak(); }
    // 判断角色是否处于移动类状态。
    bool isMove() const { return isPatrol() || isChase() || isAlert() || isRun() || isWalk() || isFinding(); }
    // 参与组件快照的状态字段。
    MG_REFLECT(RoleStatus, eEntityStatus, entityParams)
};
// 以引用计数叠加的控制限制状态。
struct ExtraState
{
    // 各限制状态的引用计数。
    std::array<int32_t, 8> refs{};
    // 判断限制状态是否生效。
    bool active(mugen::ExtraState state) const { return refs.at(static_cast<size_t>(state) - 1) > 0; }
    // 增加限制状态引用。
    void add(mugen::ExtraState state) { ++refs.at(static_cast<size_t>(state) - 1); }
    // 移除限制状态引用，禁止计数下溢。
    void remove(mugen::ExtraState state)
    {
        auto& count = refs.at(static_cast<size_t>(state) - 1);
        MG_ASSERT(count > 0 && "Extra state reference count underflow");
        --count;
    }
    // 判断角色是否可接收控制。
    bool isCanControl() const { return !active(mugen::ExtraState::Stun) && !active(mugen::ExtraState::Frozen); }
    // 判断角色是否可移动。
    bool isCanMove() const
    {
        return isCanControl() && !active(mugen::ExtraState::Durance) && !active(mugen::ExtraState::Palsy);
    }
    // 参与组件快照的引用计数。
    MG_REFLECT(ExtraState, refs)
};
// 角色基础、叠加和扩展属性。
struct Attributes
{
    // 基础属性：力量、敏捷、体质、精神、生命及战斗数值。
    std::array<Fixed, 16> basic{};
    // 临时效果叠加值。
    std::array<Fixed, 16> overlay{};
    // 以扩展属性标识索引的附加值。
    std::array<Fixed, 16> extend{};
    // 返回基础和叠加属性之和。
    Fixed value(size_t index) const { return basic.at(index) + overlay.at(index); }
    // 返回含扩展项的生命上限。
    Fixed maxHp() const { return basic[4] + extend[6]; }
    // 按扩展属性标识访问可写值。
    Fixed& extended(int32_t id) { return extend.at(static_cast<size_t>(id - 100)); }
    // 按扩展属性标识访问只读值。
    const Fixed& extended(int32_t id) const { return extend.at(static_cast<size_t>(id - 100)); }
    // 参与组件快照的属性字段。
    MG_REFLECT(Attributes, basic, overlay, extend)
};
// 角色当前资源及其上限。
struct Vitals
{
    // 当前生命值。
    Fixed hp;
    // 当前魔法值。
    Fixed mp = Fixed::fromInt(100);
    // 当前能量值。
    Fixed ep;
    // 生命上限。
    Fixed maxHp;
    // 魔法上限。
    Fixed maxMp = Fixed::fromInt(100);
    // 能量上限。
    Fixed maxEp = Fixed::fromInt(1000);
    // 魔法自然回复倍率。
    Fixed mpRecoveryScale = Fixed::fromInt(1);
    // 护盾剩余值与消耗倍率。
    Fixed m_shield, m_shieldConsumeScale = "1"_fx;
    // 锁血下限与能量回复倍率，由属性或 Buff 所有者写入。
    Fixed m_hpLock, m_epRecoveryScale = "1"_fx;
    // 是否允许损失生命或接受打击。
    bool m_beHitEnabled = true;
    // 上次角色更新采样的资源及变化比例，供事件条件读取。
    Fixed m_previousHp, m_previousEp, m_hpChangePercent;
    bool m_lostHp = false, m_recoveredHp = false, m_lostEp = false, m_recoveredEp = false;
    // 判断生命耗尽且具备有效上限。
    bool isDead() const { return hp <= Fixed{} && maxHp > Fixed{}; }
    // 清空护盾值。
    void cleanShield() { m_shield = {}; }
    // 护盾是否足以完整吸收本次变化。
    bool shieldCovers(Fixed delta) const
    {
        const auto scaled = delta * m_shieldConsumeScale;
        return m_shield > (scaled < Fixed{} ? -scaled : scaled);
    }
    // 护盾吸收伤害：护盾充足时清零伤害，耗尽时清零护盾并返回剩余扣血量。
    Fixed absorbShield(Fixed delta)
    {
        if (shieldCovers(delta))
        {
            m_shield += delta * m_shieldConsumeScale;
            return {};
        }
        const Fixed oldShield = m_shield;
        cleanShield();
        return -delta >= oldShield ? delta + oldShield : Fixed{};
    }
    // 参与组件快照的资源字段。
    MG_REFLECT(Vitals,
               hp,
               mp,
               ep,
               maxHp,
               maxMp,
               maxEp,
               mpRecoveryScale,
               m_shield,
               m_shieldConsumeScale,
               m_hpLock,
               m_epRecoveryScale,
               m_beHitEnabled,
               m_previousHp,
               m_previousEp,
               m_hpChangePercent,
               m_lostHp,
               m_recoveredHp,
               m_lostEp,
               m_recoveredEp)
};
// 角色世界坐标、朝向和缩放。
struct Transform
{
    // 三维逻辑坐标。
    FixedVec3 position;
    // 水平朝向，仅取 -1 或 1。
    int32_t towardX = 1;
    // 朝向对应的 X 方向向量。
    Fixed vectorX;
    // 朝向对应的 Z 方向向量。
    Fixed vectorZ;
    // 逻辑碰撞框缩放倍率。
    Fixed scale = Fixed::fromInt(1);
    // 参与组件快照的变换字段。
    MG_REFLECT(Transform, position, towardX, vectorX, vectorZ, scale)
};
// 可快照的值类型动作播放状态。
struct MotionState
{
    // 动作文件，更新阶段据此重建临时播放器。
    std::string motionFile;
    // 当前动作名称。
    std::string motionName;
    // 当前动作入口标识。
    std::string entryId;
    // 当前动作播放时间。
    Fixed timeMs;
    // 动作时间缩放倍率。
    Fixed actionScale = Fixed::fromInt(1);
    // 是否循环播放。
    bool loop = true;
    // 是否正在播放。
    bool playing = false;
    // 已完成循环次数。
    int64_t m_completedLoops = 0;
    // 参与组件快照的动作字段。
    MG_REFLECT(MotionState, motionFile, motionName, entryId, timeMs, actionScale, loop, playing, m_completedLoops)
};
// 标记由外部同步驱动的角色。
struct Puppet
{
    // 外部控制者标识。
    int64_t playerId = 0;
    // 参与组件快照的受控角色字段。
    MG_REFLECT(Puppet, playerId)
};
// 位于世界坐标的轴对齐碰撞框。
struct WorldBox
{
    // 框体起点。
    FixedVec3 pos;
    // 框体尺寸。
    FixedVec3 size;
    // 参与碰撞快照的框体字段。
    MG_REFLECT(WorldBox, pos, size)
};
// 角色当前帧注册的攻击和受击框。
struct ColliderState
{
    // 攻击碰撞框列表。
    std::vector<WorldBox> attack;
    // 受击碰撞框列表。
    std::vector<WorldBox> damage;
    // 本次注册发生的逻辑帧。
    uint64_t registrationFrame = 0;
    // 深度碰撞半径。
    Fixed radius;
    // 参与组件快照的碰撞体字段。
    MG_REFLECT(ColliderState, attack, damage, registrationFrame, radius)
};
// 两个碰撞体接触状态的变化类型。
enum class CollisionEvent : uint8_t
{
    // 本帧首次接触。
    Began,
    // 持续接触。
    Continue,
    // 接触已结束。
    End
};
// 一项持续接触及其接触点。
struct CollisionContact
{
    // 接触的另一实体。
    Entity other;
    // 屏幕平面中的接触点。
    FixedVec2 point;
    // 参与碰撞状态快照的接触字段。
    MG_REFLECT(CollisionContact, other, point)
};
// 用于推导接触事件的历史接触集合。
struct CollisionState
{
    // 当前仍在接触的实体。
    std::vector<CollisionContact> contacts;
    // 参与组件快照的接触集合。
    MG_REFLECT(CollisionState, contacts)
};
// 一名施加者在目标身上的嘲讽累计值。
struct SneerRecount
{
    // 施加嘲讽的角色。
    Entity m_source;
    // 当前累计权重。
    Fixed m_value;
    MG_REFLECT(SneerRecount, m_source, m_value)
};
// 角色流程使用的逻辑计时器。
struct RoleTimers
{
    // 冻结剩余毫秒。
    Fixed freezeMs;
    // 静止剩余毫秒。
    Fixed staticMs;
    // 召唤计时，-1 表示未启动。
    Fixed summonMs = Fixed::fromInt(-1);
    // 怒气累计时间。
    Fixed timeRage;
    // 怒气流程附加的 Buff 标识。
    std::vector<int32_t> rageBuffs;
    // 当前连续跑动和站立的秒数，条件读取上一帧的动作采样。
    Fixed m_movedSeconds, m_standingSeconds;
    // 各施加者对本角色累积的嘲讽权重。
    std::vector<SneerRecount> m_sneerRecounts;
    // 参与组件快照的计时字段。
    MG_REFLECT(RoleTimers,
               freezeMs,
               staticMs,
               summonMs,
               timeRage,
               rageBuffs,
               m_movedSeconds,
               m_standingSeconds,
               m_sneerRecounts)
};
// 受击和击退流程的运行时状态。
struct RoleHitState
{
    // 当前硬直值。
    Fixed rigidity;
    // 当前重量值。
    Fixed weight;
    // 当前疲劳值。
    Fixed fatigue;
    // 已累计的受击次数。
    int32_t hitCounts = 0;
    // 硬直持续时间。
    Fixed stiffTime;
    // 受击施加的强制速度。
    FixedVec3 forceVelocity;
    // 受击流程的动作时间缩放。
    Fixed m_actionScale = "1"_fx;
    // 是否已执行命中保护。
    bool m_hitProtectExecuted = false;
    // 受击保护的可选生命阈值与清理倒计时。
    std::optional<Fixed> m_hitProtectHp;
    Fixed m_hitProtectTime;
    // 尚未由 Buff 池消费的保护效果施加请求。
    std::vector<int32_t> m_protectionBuffRequests;
    // 是否被障碍物阻挡。
    bool m_obstruct = false;
    // 待触发的受击冻结定时器标识。
    uint64_t m_freezeTimer = 0;
    // 当前受击来源和伤害数字位置。
    Entity m_beHitEffect;
    FixedVec2 m_contact;
    // 命中期间临时伤害、暴击与累计输出。
    Fixed m_damage, m_hurt, m_dps;
    bool m_critical = false, m_beCritical = false;
    // 连击累计经验与缩放。
    Fixed m_comboExp, m_comboScale = "1"_fx;
    // 水晶的一次性命中标记。
    bool m_crystalHit = false;
    // 正配置总值耗尽才触发浮空保护，零配置不表示永久保护。
    bool isRigidity(Fixed configuredRigidity) const
    {
        return rigidity <= Fixed{} && configuredRigidity > Fixed{};
    }
    // 读取硬直时逐次扣除疲劳，死亡或未触发保护时保持原值。
    Fixed getStiffTime(Fixed configuredRigidity, bool dead)
    {
        if (!dead && isRigidity(configuredRigidity))
            stiffTime = std::max(Fixed{}, stiffTime - fatigue * Fixed::fromInt(hitCounts));
        return stiffTime;
    }
    // 首次命中记录当前生命减去最大生命比例后的阈值。
    void initHitProtect(bool pvp, Fixed hp, Fixed maxHp, Fixed hpScale, int32_t cleanTime)
    {
        if (!pvp || m_hitProtectHp) return;
        m_hitProtectHp = std::max(Fixed{}, hp - maxHp * hpScale);
        m_hitProtectTime = Fixed::fromInt(cleanTime);
        m_hitProtectExecuted = false;
    }
    // 已执行的保护不再因追击延长。
    void updateHitProtect(int32_t cleanTime)
    {
        if (m_hitProtectHp && !m_hitProtectExecuted)
            m_hitProtectTime = Fixed::fromInt(cleanTime);
    }
    // 清除本轮保护状态，下一次命中可重新建立阈值。
    void resetHitProtect()
    {
        m_hitProtectHp.reset();
        m_hitProtectTime = {};
        m_hitProtectExecuted = false;
    }
    // 参与组件快照的受击字段。
    MG_REFLECT(RoleHitState,
               rigidity,
               weight,
               fatigue,
               hitCounts,
               stiffTime,
               forceVelocity,
               m_actionScale,
               m_hitProtectExecuted,
               m_obstruct,
               m_freezeTimer,
               m_beHitEffect,
               m_contact,
               m_damage,
               m_hurt,
               m_dps,
               m_critical,
               m_beCritical,
               m_comboExp,
               m_comboScale,
               m_crystalHit,
               m_hitProtectHp,
               m_hitProtectTime,
               m_protectionBuffRequests)
};
}  // namespace mugen::sim
