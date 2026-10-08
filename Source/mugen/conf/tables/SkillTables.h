#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
struct SkillAttackConfig
{
    std::vector<std::vector<int32_t>> actionIds;
    int64_t cd      = 0;
    int32_t cdCount = 0;
    int32_t crystal = 0;
    int32_t descId  = 0;
    int32_t ep      = 0;
    std::variant<int32_t, std::string> icon;
    int32_t id        = 0;
    int32_t mp        = 0;
    int32_t nameId    = 0;
    int32_t nextSkill = 0;
    int32_t pressTime = 0;
    int64_t pvpCd     = 0;
    int32_t rage      = 0;
    int32_t skillAiId = 0;
    int32_t skillType = 0;
    int32_t sorder    = 0;
    std::vector<int32_t> sorderControlType;
    int32_t type = 0;
    std::vector<std::vector<int32_t>> upActionIds;

    MG_REFLECT(SkillAttackConfig,
               actionIds,
               cd,
               cdCount,
               crystal,
               descId,
               ep,
               icon,
               id,
               mp,
               nameId,
               nextSkill,
               pressTime,
               pvpCd,
               rage,
               skillAiId,
               skillType,
               sorder,
               sorderControlType,
               type,
               upActionIds)
};

struct SkillHitConfig
{
    int32_t airDisplacementId       = 0;
    int32_t displacementId          = 0;
    int32_t floorDisplacementId     = 0;
    int32_t freezeTime              = 0;
    int32_t freezeTimeControlEffect = 0;
    int32_t freezeTimeControlRole   = 0;
    int32_t freezeTimeDelay         = 0;
    int32_t hitCondition            = 0;
    int32_t hitCounts               = 0;
    int32_t hitInterval             = 0;
    int32_t hitMust                 = 0;
    Fixed hitRigidity;
    int32_t hitType = 0;
    Fixed hurtRate;
    int32_t hurtType = 0;
    int32_t id       = 0;
    Fixed pvpHurtRate;
    int32_t stiffTime = 0;

    MG_REFLECT(SkillHitConfig,
               airDisplacementId,
               displacementId,
               floorDisplacementId,
               freezeTime,
               freezeTimeControlEffect,
               freezeTimeControlRole,
               freezeTimeDelay,
               hitCondition,
               hitCounts,
               hitInterval,
               hitMust,
               hitRigidity,
               hitType,
               hurtRate,
               hurtType,
               id,
               pvpHurtRate,
               stiffTime)
};
}  // namespace mugen::conf
