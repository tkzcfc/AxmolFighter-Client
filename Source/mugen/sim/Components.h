#pragma once
#include "mugen/sim/role/RoleComponents.h"
#include "mugen/sim/motion/MotionComponents.h"
#include "mugen/sim/anim/AnimComponents.h"
#include "mugen/sim/intent/IntentComponents.h"
#include "mugen/sim/stats/StatsComponents.h"
#include "mugen/sim/stats/StatusFlags.h"
#include "mugen/sim/combat/CombatComponents.h"
#include "mugen/sim/skill/SkillComponents.h"
#include "mugen/sim/effect/EffectComponents.h"
#include "mugen/sim/buff/BuffComponents.h"
#include "mugen/sim/base/SpawnStage.h"
#include "mugen/sim/Events.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/reflect/Reflect.h"
#include <optional>
#include <tuple>

namespace mugen::sim
{
// 出生时间戳（R3）：记录出生帧与出生同步点，用于出生帧内的行动门控。
struct BornStamp
{
    // 出生帧号。
    uint64_t m_frame = 0;
    // 出生同步点。
    SpawnStage m_stage = SpawnStage::RoleTimeline;
    MG_REFLECT(BornStamp, m_frame, m_stage)
};
// 实体的静态身份与阵营信息。
struct Identity
{
    // 实体种类。
    SpawnKind m_kind = SpawnKind::Role;
    // 配置标识。
    int32_t m_configId = 0;
    // 所属阵营位。
    int32_t m_camp = 2;
    // 可敌对阵营的位掩码。
    int32_t m_hostileMask = 0;
    // 角色类型标识。
    int32_t m_roleType = 1;
    // 角色等级。
    int32_t m_level = 1;
    // 可选的控制玩家槽位。
    std::optional<uint32_t> m_playerSlot;
    // 召唤归属句柄。
    Entity m_owner;
    // 召唤最终归属句柄。
    Entity m_rootOwner;
    // 出生时间戳。
    BornStamp m_born;
    // 同配置的生成序号（语义键 role:<configId>#<n>）。
    uint32_t m_ordinal = 0;
    MG_REFLECT(Identity, m_kind, m_configId, m_camp, m_hostileMask, m_roleType, m_level, m_playerSlot, m_owner,
               m_rootOwner, m_born, m_ordinal)
};
// 标签组件：变身后休眠、跨房缓冲、房间挂起；所有系统遍历时跳过。
struct Inactive
{
    uint8_t m_tag = 0;
    MG_REFLECT(Inactive, m_tag)
};
// 标签组件：本帧待回收（R4）；打上后本帧其余系统一律跳过，物理回收在 FlushSystem。
struct Despawning
{
    uint8_t m_tag = 0;
    MG_REFLECT(Despawning, m_tag)
};
}  // namespace mugen::sim

// 新 sim 组件唯一登记表，顺序固定序列化类型编号（上限 64）。
#define MG_COMPONENT_LIST(X)                                                                                          \
    X(Inactive) X(Despawning) X(Identity) X(PlayerControl) X(Intent) X(Transform) X(Displacement) X(MotionState)      \
        X(Hitboxes) X(Stats) X(StatusFlags) X(Health) X(Pause) X(RoleState) X(HitState) X(SkillBook) X(SkillCast)      \
            X(ActionTimeline) X(EffectInfo) X(HitTracker) X(Lifetime) X(Follow) X(ContactMemory) X(BuffList) \
                X(AbilityList) X(TauntTable) X(ComboMeter) X(RageState)

namespace mugen::sim
{
#define MG_SIM_COMPONENT_TUPLE(T) \
    , std::tuple<T> {}
// 由登记表展开的 ECS 组件元组。
using Components = decltype(std::tuple_cat(std::tuple<> {} MG_COMPONENT_LIST(MG_SIM_COMPONENT_TUPLE)));
#undef MG_SIM_COMPONENT_TUPLE
}  // namespace mugen::sim
