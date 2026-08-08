#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/ecs/Types.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

// 命中信息快照：由 CombatSystem 在命中发生时写入防守方组件，
// BehaviorSystem 通过 update() 消费。
// 所有需要在命中时快照的数据均存于此，保证时序安全，避免消费时依赖攻击者当前状态。
class PendingHitInfo : public Object
{
public:
    typedef Object Super;

public:
    PendingHitInfo() {}
    virtual ~PendingHitInfo() {}

    // 攻击者实体 ID
    EntityId attackerId = INVALID_ENTITY_ID;
    // 命中类型，同时作为多命中优先级依据（枚举值越大优先级越高）
    HitType hitType = HitType::kHitNone;
    // 受击状态名（为空时使用默认 Stun）
    std::string hitState;
    // 经公式计算后的最终受击硬直时间（毫秒）；0 表示退化为 AnimEnd 行为
    int32_t hitstunMs = 0;
    // 预计算的 X 轴冲量（已含方向符号，CombatSystem 写入时确定）
    float impulseX = 0.0f;
    // 预计算的 Z 轴冲量（击飞高度）
    float impulseZ = 0.0f;

    MG_DEFINE_SERIALIZABLE(attackerId, hitType, hitState, hitstunMs, impulseX, impulseZ)
};

class SkillStateComponent : public Component
{
public:
    typedef Component Super;

public:
    SkillStateComponent() {}
    virtual ~SkillStateComponent() {}

public:
    // 待处理的命中信息队列：CombatSystem 写入，BehaviorSystem 消费。
    // 使用队列而非单值，支持同一帧内多个攻击者命中同一目标的情况。
    std::vector<PendingHitInfo> pendingHits;

    // 当前受击硬直剩余时间（毫秒）。
    int32_t activeHitstunMs = 0;

    // 当前生效的受击类型。
    // 处于 HitState 时，只有 hitType 严格大于此字段的新攻击才能打断当前硬直。
    HitType activeHitType = HitType::kHitNone;

    MG_DEFINE_SERIALIZABLE(pendingHits, activeHitstunMs, activeHitType)
};

NS_MG_END
