#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// skill 表映射的技能施放配置。
struct SkillAttackConfig
{
    // 起手和抬起时的动作序列。
    std::vector<std::vector<int32_t>> actionIds;
    // 冷却、消耗、文本与图标配置。
    int64_t cd      = 0;
    int32_t cdCount = 0;
    int32_t crystal = 0;
    int32_t descId  = 0;
    int32_t ep      = 0;
    std::variant<int32_t, std::string> icon;
    // 标识、后继技能、蓄力和 PVP 冷却。
    int32_t id        = 0;
    int32_t mp        = 0;
    int32_t nameId    = 0;
    int32_t nextSkill = 0;
    int32_t pressTime = 0;
    int64_t pvpCd     = 0;
    // 怒气、决策、触发方式、打断优先级与技能类型。
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

// skill_hit 表映射的命中反应配置。
struct SkillHitConfig
{
    // 空中、地面和倒地位移配置 id。
    int32_t airDisplacementId   = 0;
    int32_t displacementId      = 0;
    int32_t floorDisplacementId = 0;
    // 命中冻结时长及受影响对象。
    int32_t freezeTime              = 0;
    int32_t freezeTimeControlEffect = 0;
    int32_t freezeTimeControlRole   = 0;
    int32_t freezeTimeDelay         = 0;
    // 命中条件、次数、间隔、必中和韧性参数。
    int32_t hitCondition = 0;
    int32_t hitCounts    = 0;
    int32_t hitInterval  = 0;
    int32_t hitMust      = 0;
    Fixed hitRigidity;
    // 击飞类型、伤害倍率、标识与硬直时长。
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

// skill_hurt 表映射的等级伤害标准配置。
struct SkillHurtConfig
{
    // 攻击、暴击、闪避标准和伤害值。
    int32_t atkStandard        = 0;
    int32_t critDamageStandard = 0;
    int32_t critStandard       = 0;
    int32_t dodgeStandard      = 0;
    int32_t hurt               = 0;
    int32_t id                 = 0;
    int32_t monsterHurt        = 0;

    MG_REFLECT(SkillHurtConfig, atkStandard, critDamageStandard, critStandard, dodgeStandard, hurt, id, monsterHurt)
};

// skill_ai 表映射的自动施法条件配置。
struct SkillAiConfig
{
    // 检查间隔、条件组合、标识和首次检查冷却。
    int64_t checkCd = 0;
    std::vector<std::vector<int32_t>> composition;
    int32_t id     = 0;
    int32_t loadCd = 0;
    // 目标状态、距离、技能和概率条件。
    int32_t oppCombo = 0;
    std::vector<int32_t> oppDisX;
    std::vector<int32_t> oppDisZ;
    int32_t oppSkillId = 0;
    int32_t oppStatus  = 0;
    int32_t prob       = 0;
    // 自身生命、状态及可使用次数条件。
    std::vector<int32_t> selfHp;
    int32_t selfStatus = 0;
    int32_t useCount   = 0;

    MG_REFLECT(SkillAiConfig,
               checkCd,
               composition,
               id,
               loadCd,
               oppCombo,
               oppDisX,
               oppDisZ,
               oppSkillId,
               oppStatus,
               prob,
               selfHp,
               selfStatus,
               useCount)
};

// skill_rock 表映射的指令演出配置。
struct SkillRockConfig
{
    // 指令序列、技能、Spine 动作和视频资源。
    int32_t commandIndex = 0;
    std::vector<int32_t> commands;
    int32_t id = 0;
    Fixed skillDamageFactor;
    int32_t skillId = 0;
    std::string spineAction;
    int32_t spineId = 0;
    int32_t videoId = 0;

    MG_REFLECT(SkillRockConfig, commandIndex, commands, id, skillDamageFactor, skillId, spineAction, spineId, videoId)
};

// skill_tree 表映射的技能树配置。
struct SkillTreeConfig
{
    // 描述文本与树标识。
    int32_t descId = 0;
    int32_t id     = 0;
    // 被动、主动与搓招叶子，以及搓招叶子的界面槽位。
    std::vector<int32_t> leafBuffId;
    std::vector<int32_t> leafIds;
    std::vector<int32_t> leafRubId;
    std::vector<int32_t> leafRubPlace;
    // 名称文本、职业背景、职业类型、树类型与界面名。
    int32_t nameId = 0;
    std::string professionBackground;
    int32_t professionType = 0;
    int32_t treeType       = 0;
    std::string ui;

    MG_REFLECT(SkillTreeConfig,
               descId,
               id,
               leafBuffId,
               leafIds,
               leafRubId,
               leafRubPlace,
               nameId,
               professionBackground,
               professionType,
               treeType,
               ui)
};

// skill_leaf 表映射的技能树叶子配置。
struct SkillLeafConfig
{
    // 攻击类型、连段与碎片列表。碎片表未导入。
    int32_t attackType = 0;
    int32_t chain      = 0;
    std::vector<int32_t> chipIds;
    // 描述、伤害类型、图标、标识与名称。
    int32_t descId   = 0;
    int32_t hurtType = 0;
    std::string icon;
    int32_t id     = 0;
    int32_t nameId = 0;
    // 溢出等级与伙伴解锁。
    int32_t overflowLevelLimit = 0;
    int32_t partnerUnlock      = 0;
    // 伤害系数、关联技能、技能类型与标签。
    Fixed skillDamageFactor;
    int32_t skillId   = 0;
    int32_t skillType = 0;
    std::vector<int32_t> tag;
    // 解锁条件：前置叶子、前置等级与角色等级。
    std::vector<int32_t> unlockLeafIds;
    std::vector<int32_t> unlockLeafLevels;
    int32_t unlockRoleLevel = 0;
    // 升级消耗、等级上限、等级步长与演示视频。
    int32_t upgradeConsumeFactor = 0;
    int32_t upgradeLevelLimit    = 0;
    int32_t upgradeLevelStep     = 0;
    int32_t videoId              = 0;

    MG_REFLECT(SkillLeafConfig,
               attackType,
               chain,
               chipIds,
               descId,
               hurtType,
               icon,
               id,
               nameId,
               overflowLevelLimit,
               partnerUnlock,
               skillDamageFactor,
               skillId,
               skillType,
               tag,
               unlockLeafIds,
               unlockLeafLevels,
               unlockRoleLevel,
               upgradeConsumeFactor,
               upgradeLevelLimit,
               upgradeLevelStep,
               videoId)
};

// skill_change_system 表映射的技能切换配置。
struct SkillChangeSystemConfig
{
    // 技能和技能组的切换条件、目标及持续时间。
    int32_t id                 = 0;
    int32_t skillChange        = 0;
    int32_t skillChangeId      = 0;
    int32_t skillChangeTime    = 0;
    int32_t skillGroup         = 0;
    int32_t skillGroupChangeId = 0;
    int32_t skillGroupTime     = 0;
    int32_t skillId            = 0;
    int32_t skillNewId         = 0;

    MG_REFLECT(SkillChangeSystemConfig,
               id,
               skillChange,
               skillChangeId,
               skillChangeTime,
               skillGroup,
               skillGroupChangeId,
               skillGroupTime,
               skillId,
               skillNewId)
};
}  // namespace mugen::conf
