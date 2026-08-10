#pragma once

#include "mugen/core/StdC.h"

NS_MG_BEGIN

/**
 * Buff 事件枚举：数值与配置表 began/ended 对齐。
 */
enum class BFEvent : int32_t
{
    BeforePrepareSkill = 1,
    BeforeCastSkill    = 2,
    BeforeHit          = 3,
    AfterHit           = 4,
    BeforeToBeHit      = 5,
    AfterToBeHit       = 6,
    AfterCastSkill     = 7,
    AfterPrepareSkill  = 8,
    BeforeNextSkill    = 9,
    AfterNextSkill     = 10,
    BeforeCrazy        = 11,
    AfterCrazy         = 12,
    EvadeSuccess       = 13,
    BeforeDeath        = 14,
    EpZero             = 15,
    UseTp              = 16,
    ShieldClean        = 17,
    AttackMiss         = 18,
    SkillColdStart     = 19,
    SkillColdEnd       = 20,
    HpChange           = 21,
    UseEp              = 22,
    BeforeArtifactSkill = 23,

    BehaviorStateStart = 50,
    BehaviorStateEnd   = 51,

    Enter       = 100,
    Exit        = 101,
    OnceUpdate  = 102,
};

NS_MG_END
