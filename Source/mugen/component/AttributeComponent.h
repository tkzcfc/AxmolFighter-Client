#pragma once

#include "mugen/buff/ExtendAttribute.h"
#include "mugen/core/ecs/Component.h"
#include "mugen/core/Object.h"

NS_MG_BEGIN

// 战斗属性烘焙结果（对齐参照 AttributeName 16 槽；非配置表）
class CombatStats : public Object
{
public:
    typedef Object Super;

public:
    CombatStats() {}
    virtual ~CombatStats() {}

    float sourceForce = 0;
    float agility     = 0;
    float habitus     = 0;
    float spirit      = 0;
    // AttributeName "hp"：最大生命
    float hpMax = 0;
    float atk   = 0;
    float def   = 0;
    float matk  = 0;
    float mdef  = 0;
    float crit             = 0;
    float critResist       = 0;
    float critDamage       = 0;
    float critDamageResist = 0;
    float dodge      = 0;
    float hit        = 0;
    float baseDamage = 0;

    MG_DEFINE_SERIALIZABLE(sourceForce,
                           agility,
                           habitus,
                           spirit,
                           hpMax,
                           atk,
                           def,
                           matk,
                           mdef,
                           crit,
                           critResist,
                           critDamage,
                           critDamageResist,
                           dodge,
                           hit,
                           baseDamage)
};

// 运行时属性（对齐 EntityAttribute：basic + 当前 vitals；非配置表）
class AttributeComponent : public Component
{
public:
    typedef Component Super;

public:
    AttributeComponent();
    virtual ~AttributeComponent();

public:
    CombatStats basic;

    // 当前 vitals（≈ tEntityExtendValue）
    float hp    = 0.0f;
    float mp    = 0.0f;
    float ep    = 0.0f;
    float mpMax = 140.0f;
    float epMax = 100.0f;

    // 移速（来自 RoleConfig.velocity*1000，可被 Buff 改）
    float moveSpeed = 0.0f;

    int32_t crystal      = 0;
    float mpConsumeScale = 1.0f;
    float epConsumeScale = 1.0f;
    float epPlus         = 0.0f;

    ExtendAttribute extendAttribute;

    int32_t freezeRemainingMs = 0;
    int32_t freezeDelayMs     = 0;

    MG_DEFINE_SERIALIZABLE(basic,
                           hp,
                           mp,
                           ep,
                           mpMax,
                           epMax,
                           moveSpeed,
                           crystal,
                           mpConsumeScale,
                           epConsumeScale,
                           epPlus,
                           extendAttribute,
                           freezeRemainingMs,
                           freezeDelayMs)
};

NS_MG_END
