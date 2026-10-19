#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

// 配置允许的规则类名；枚举和名称表共用唯一清单，保留源表顺序。
#define MG_BUFF_RULE_NAMES(X)        \
    X(BuffSuperArmor)                \
    X(BuffCrazy)                     \
    X(BuffInvincible)                \
    X(BuffBurn)                      \
    X(BuffPoison)                    \
    X(BuffSneer)                     \
    X(BuffStun)                      \
    X(BuffHPLock)                    \
    X(BuffSpeed)                     \
    X(BuffBleeding)                  \
    X(BuffDurance)                   \
    X(BuffDark)                      \
    X(BuffThunder1)                  \
    X(BuffThunder2)                  \
    X(BuffThunder3)                  \
    X(BuffPlague1)                   \
    X(BuffPlague2)                   \
    X(BuffPlague3)                   \
    X(BuffBreakArmor)                \
    X(BuffMagicUp)                   \
    X(BuffDark2)                     \
    X(BuffSeparate)                  \
    X(BuffCold)                      \
    X(BuffPawMark)                   \
    X(BuffDeathMark)                 \
    X(BuffPurplePoison)              \
    X(BuffPassivation)               \
    X(BuffBlind)                     \
    X(BuffRepent)                    \
    X(BuffFall)                      \
    X(BuffPalsy)                     \
    X(BuffBurn2)                     \
    X(BuffChill)                     \
    X(BuffFlaw)                      \
    X(BuffStarShield)                \
    X(BuffGhostWalk)                 \
    X(BuffGuard)                     \
    X(BuffBloodBeast)                \
    X(BuffFlames)                    \
    X(BuffRedPython)                 \
    X(BuffMeltingArmor)              \
    X(BuffKillIntent)                \
    X(BuffCutSakura)                 \
    X(BuffSilence)                   \
    X(BuffLightMark)                 \
    X(BuffStoneSkin)                 \
    X(BuffAdjudication)              \
    X(BuffReverse)                   \
    X(BuffElectricEye)               \
    X(BuffLightWing)                 \
    X(BuffSpeedStep)                 \
    X(BuffMercenary)                 \
    X(BuffUnstable)                  \
    X(BuffDarkMark)                  \
    X(BuffSuperBlind)                \
    X(BuffMight)                     \
    X(BuffElectron)                  \
    X(BuffElectron2)                 \
    X(BuffStealth)                   \
    X(BuffFrozen)                    \
    X(BuffBackfire)                  \
    X(BuffBackfireHurt)              \
    X(BuffDarkMark2)                 \
    X(BuffAddByApplicator)           \
    X(BuffAddByApplicator2)          \
    X(BuffAddByApplicator3)          \
    X(BuffAddByApplicator4)          \
    X(BuffAddBySelect)               \
    X(BuffAddByCounter)              \
    X(BuffAddByDetector)             \
    X(BuffRemoveByType)              \
    X(BuffRemoveByTypeAddExtra)      \
    X(BuffAddByState)                \
    X(BuffAddByComboEvaluate)        \
    X(BuffRemoveByRule)              \
    X(BuffRemoveById)                \
    X(BuffRemoveById2)               \
    X(BuffAttributeCombo)            \
    X(BuffAttributeSlotCD)           \
    X(BuffDamageHurt)                \
    X(BuffDamageHurt2)               \
    X(BuffDamageHurt3)               \
    X(BuffDamageCrit)                \
    X(BuffDamageCritDef)             \
    X(BuffDamageSlot)                \
    X(BuffDamageEndLink)             \
    X(BuffDamageReduction)           \
    X(BuffDamageHPLoss)              \
    X(BuffDamageCritHPLoss)          \
    X(BuffDamageHPResidue)           \
    X(BuffDamageTPLoss)              \
    X(BuffCurtTPLoss)                \
    X(BuffDamageTPResidue)           \
    X(BuffCritEPPercent)             \
    X(BuffHurtEPPercent)             \
    X(BuffDamageTypeGradation)       \
    X(BuffHPTypeGradation)           \
    X(BuffCritTypeGradation)         \
    X(BuffCritDamageTypeGradation)   \
    X(BuffDefendTypeGradation)       \
    X(BuffDefendHPLoss)              \
    X(BuffDefendHPResidue)           \
    X(BuffDamageBySpeed)             \
    X(BuffDamageCritByHPLost)        \
    X(BuffFloorTm)                   \
    X(BuffHP)                        \
    X(BuffModifyHPBasedByCurrentHp)  \
    X(BuffModifyHPBasedByLostHp)     \
    X(BuffHPMAX)                     \
    X(BuffHealingChange)             \
    X(BuffEP)                        \
    X(BuffEPConsumeScale)            \
    X(BuffEPRecoveryScale)           \
    X(BuffCPConsumeScale)            \
    X(BuffEPPlus)                    \
    X(BuffTP)                        \
    X(BuffTpAddNewBuff)              \
    X(BuffTPConsumeScale)            \
    X(BuffTPConsumeScaleSlot)        \
    X(BuffTPRecoveryScale)           \
    X(BuffTPRecoveryScaleBySpeed)    \
    X(BuffTPRecoveryScaleBySlotCD)   \
    X(BuffCDSkill)                   \
    X(BuffUpdateArtifactCD)          \
    X(BuffCDSlot)                    \
    X(BuffHemophagia)                \
    X(BuffModifyCDSkill)             \
    X(BuffConsumeCrystal)            \
    X(BuffCrit)                      \
    X(BuffCritHPLoss)                \
    X(BuffSign)                      \
    X(BuffDeductEpReplaceHp)         \
    X(BuffDeductTpReplaceHp)         \
    X(BuffImmune)                    \
    X(BuffComboExp)                  \
    X(BuffExtendDebuff)              \
    X(BuffNotConsumeCrystal)         \
    X(BuffShieldConsumeScale)        \
    X(BuffShield)                    \
    X(BuffAvoidCrit)                 \
    X(BuffImmuneDebuff)              \
    X(BuffLoopCheck)                 \
    X(BuffWithTypeAddBuff)           \
    X(BuffLoopCheck2)                \
    X(BuffWithTypeAddBuff2)          \
    X(BuffSpecialAbilityApplicator)  \
    X(BuffDodge)                     \
    X(BuffHit)                       \
    X(BuffResetSkillCD)              \
    X(BuffResetArtifactCD)           \
    X(BuffModifyCDSkillNew)          \
    X(BuffCDArtifact)                \
    X(BuffAngerAdd)                  \
    X(BuffAngerAddScale)             \
    X(BuffWhiteFrame)                \
    X(BuffSkyProtect)                \
    X(BuffDisableArt)                \
    X(BuffDisableSlots)              \
    X(BuffChangeRoleAcitonScaleTime) \
    X(BuffSealSkill)                 \
    X(BuffSummonDummy)               \
    X(BuffDummyDeathRattle)

namespace mugen::conf
{
// 规则类的稠密标识，不等同于配置表的数值主键。
enum class BuffRuleId : uint16_t
{
#define MG_BUFF_RULE_ENUM(name) name,
    MG_BUFF_RULE_NAMES(MG_BUFF_RULE_ENUM)
#undef MG_BUFF_RULE_ENUM
};

// 稠密标识对应的只读类名；未移植规则仍是合法配置。
inline constexpr auto kBuffRuleNames = std::array{
#define MG_BUFF_RULE_NAME(name) std::string_view{#name},
    MG_BUFF_RULE_NAMES(MG_BUFF_RULE_NAME)
#undef MG_BUFF_RULE_NAME
};
static_assert(kBuffRuleNames.size() == 162);

// 严格匹配配置类名，未知名称不降级为未移植规则。
constexpr std::optional<BuffRuleId> buffRuleIdFromName(std::string_view name)
{
    for (size_t index = 0; index < kBuffRuleNames.size(); ++index)
    {
        if (kBuffRuleNames[index] == name)
            return static_cast<BuffRuleId>(index);
    }
    return std::nullopt;
}
}  // namespace mugen::conf

#undef MG_BUFF_RULE_NAMES
