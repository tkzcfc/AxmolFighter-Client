#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
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
    // 溢出等级。
    int32_t overflowLevelLimit = 0;
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
}  // namespace mugen::conf
