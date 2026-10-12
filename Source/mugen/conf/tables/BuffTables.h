#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// buff 表映射的状态效果配置。
struct BuffConfig
{
    // 叠加、区域、音效、事件和绑定参数。
    int32_t addType = 0;
    std::vector<int32_t> areaSetting;
    int32_t audioId              = 0;
    int32_t began                = 0;
    int32_t bindSpecialAbilityId = 0;
    int32_t binding              = 0;
    int32_t buffDirection        = 0;
    int32_t buffPartner          = 0;
    int32_t buffType             = 0;
    // 冷却、条件、描述、销毁和执行参数。
    Fixed cd;
    Fixed cdPvp;
    std::vector<int32_t> condition;
    std::vector<std::vector<Fixed>> conditionParam;
    int32_t descId      = 0;
    int32_t destroyType = 0;
    int32_t ended       = 0;
    std::vector<int32_t> eventParam;
    int32_t executeType = 0;
    int32_t hurtType    = 0;
    // 图标、标识、继承、内部冷却与周期参数。
    std::variant<int32_t, std::string> icon;
    int32_t iconDescId = 0;
    int32_t id         = 0;
    std::vector<int32_t> inherit;
    Fixed innerCd;
    Fixed interval;
    int32_t nameId = 0;
    std::vector<Fixed> paramValue;
    std::vector<std::vector<Fixed>> paramValue2;
    // 优先级、概率、层数、规则及提示参数。
    int32_t priority = 0;
    Fixed probability;
    int32_t probabilityRepeat = 0;
    int32_t removeRepeatAll   = 0;
    int32_t repeatMax         = 0;
    int32_t resetType         = 0;
    int32_t ruleId            = 0;
    int32_t showTips          = 0;
    // 表现 Spine、偏移、层级和目标参数。
    int32_t spineId           = 0;
    int32_t spineLayer        = 0;
    std::vector<int32_t> spineOffsets;
    std::vector<int32_t> spineStep;
    int32_t subType = 0;
    int32_t target  = 0;
    int32_t times   = 0;

    MG_REFLECT(BuffConfig,
               addType,
               areaSetting,
               audioId,
               began,
               bindSpecialAbilityId,
               binding,
               buffDirection,
               buffPartner,
               buffType,
               cd,
               cdPvp,
               condition,
               conditionParam,
               descId,
               destroyType,
               ended,
               eventParam,
               executeType,
               hurtType,
               icon,
               iconDescId,
               id,
               inherit,
               innerCd,
               interval,
               nameId,
               paramValue,
               paramValue2,
               priority,
               probability,
               probabilityRepeat,
               removeRepeatAll,
               repeatMax,
               resetType,
               ruleId,
               showTips,
               spineId,
               spineLayer,
               spineOffsets,
               spineStep,
               subType,
               target,
               times)
};

// buff_rule 表映射的状态效果规则配置。
struct BuffRuleConfig
{
    // 状态类型、规则类名、时装提示与标识。
    int32_t buffType = 0;
    std::string className;
    int32_t fashionShowText = 0;
    int32_t id              = 0;

    MG_REFLECT(BuffRuleConfig, buffType, className, fashionShowText, id)
};

// special_ability 表映射的特殊能力配置。
struct SpecialAbilityConfig
{
    // 冷却、绑定状态与触发条件参数。
    Fixed SpecialAbilityCD;
    Fixed SpecialAbilityCDPvp;
    std::vector<int32_t> bindBuffId;
    std::vector<int32_t> bindBuffType;
    int32_t bindSkillType  = 0;
    int32_t buffPartner    = 0;
    int32_t condition      = 0;
    int32_t conditionValue = 0;
    int32_t descId         = 0;
    // 特效、结束条件、事件、图标和标识参数。
    std::vector<int32_t> effectId;
    std::vector<int32_t> effectIdType;
    int32_t endedType = 0;
    Fixed endedValue;
    std::vector<int32_t> event;
    std::variant<int32_t, std::string> icon;
    int32_t id         = 0;
    // 周期、文本、数值、优先级、概率及 PVP 参数。
    int64_t interval   = 0;
    int32_t nameId     = 0;
    int32_t paramValue = 0;
    int32_t priority   = 0;
    Fixed probability;
    int32_t pvpSetting    = 0;
    int32_t resSoundId    = 0;
    int32_t ruleId        = 0;
    int32_t showTips      = 0;
    // 表现 Spine、规则、提示、偏移与子类型参数。
    int32_t spineId       = 0;
    int32_t spineInterval = 0;
    std::vector<int32_t> spineOffsets;
    std::vector<int32_t> spineStep;
    int32_t subType = 0;

    MG_REFLECT(SpecialAbilityConfig,
               SpecialAbilityCD,
               SpecialAbilityCDPvp,
               bindBuffId,
               bindBuffType,
               bindSkillType,
               buffPartner,
               condition,
               conditionValue,
               descId,
               effectId,
               effectIdType,
               endedType,
               endedValue,
               event,
               icon,
               id,
               interval,
               nameId,
               paramValue,
               priority,
               probability,
               pvpSetting,
               resSoundId,
               ruleId,
               showTips,
               spineId,
               spineInterval,
               spineOffsets,
               spineStep,
               subType)
};
}  // namespace mugen::conf
