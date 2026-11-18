#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// attribute 表映射的角色属性曲线配置。
struct AttributeConfig
{
    // 主属性、攻击、防御、暴击和命中相关数值。
    int32_t agility    = 0;
    int32_t atk        = 0;
    int32_t baseDamage = 0;
    Fixed bossHpRate;
    Fixed bossHurtRate;
    int32_t crit             = 0;
    int32_t critDamage       = 0;
    int32_t critDamageResist = 0;
    int32_t critResist       = 0;
    int32_t def              = 0;
    int32_t dodge            = 0;
    // 精英与首领生命、伤害倍率及受击次数参数。
    Fixed eliteHpRate;
    Fixed eliteHurtRate;
    int32_t habitus = 0;
    int32_t hit     = 0;
    int64_t hp      = 0;
    int32_t id      = 0;
    int32_t matk    = 0;
    int32_t mdef    = 0;
    Fixed monsterHitNumber;
    int32_t playerHitNumber = 0;
    int32_t sourceForce     = 0;
    int32_t spirit          = 0;

    MG_REFLECT(AttributeConfig,
               agility,
               atk,
               baseDamage,
               bossHpRate,
               bossHurtRate,
               crit,
               critDamage,
               critDamageResist,
               critResist,
               def,
               dodge,
               eliteHpRate,
               eliteHurtRate,
               habitus,
               hit,
               hp,
               id,
               matk,
               mdef,
               monsterHitNumber,
               playerHitNumber,
               sourceForce,
               spirit)
};

// obstacle 表映射的障碍物实体配置。
struct ObstacleConfig
{
    // 属性、死亡效果、受击、位置、Spine 和层级参数。
    std::vector<int32_t> attributeRate;
    int32_t deathDisplacementId = 0;
    int32_t deathEffectId       = 0;
    int32_t hitCounts           = 0;
    int32_t id                  = 0;
    int32_t obstacleType        = 0;
    int32_t radius              = 0;
    std::vector<int32_t> relativePosition;
    int32_t resSpineId    = 0;
    int32_t resSpineIdExt = 0;
    Fixed shadow;
    std::vector<int32_t> spineRelativePosition;
    int32_t tier     = 0;
    int32_t tierExt  = 0;
    int32_t velocity = 0;

    MG_REFLECT(ObstacleConfig,
               attributeRate,
               deathDisplacementId,
               deathEffectId,
               hitCounts,
               id,
               obstacleType,
               radius,
               relativePosition,
               resSpineId,
               resSpineIdExt,
               shadow,
               spineRelativePosition,
               tier,
               tierExt,
               velocity)
};

// goods 表映射的掉落物实体配置。
struct GoodsConfig
{
    // 效果、文本、显示、位置、Spine、音效和层级参数。
    int32_t buffId      = 0;
    int32_t id          = 0;
    int32_t nameId      = 0;
    int32_t nameShow    = 0;
    int32_t qualityShow = 0;
    int32_t radius      = 0;
    std::vector<int32_t> relativePosition;
    int32_t resSpineId    = 0;
    int32_t resSpineIdExt = 0;
    Fixed shadow;
    int32_t soundDrop        = 0;
    int32_t soundPick        = 0;
    int32_t specialabilityId = 0;
    std::vector<int32_t> spineRelativePosition;
    int32_t summonId = 0;
    int32_t tier     = 0;
    std::vector<int32_t> tierExt;
    int32_t velocity = 0;
    std::vector<int32_t> vertexPos;

    MG_REFLECT(GoodsConfig,
               buffId,
               id,
               nameId,
               nameShow,
               qualityShow,
               radius,
               relativePosition,
               resSpineId,
               resSpineIdExt,
               shadow,
               soundDrop,
               soundPick,
               specialabilityId,
               spineRelativePosition,
               summonId,
               tier,
               tierExt,
               velocity,
               vertexPos)
};

// npc 表映射的 NPC 实体配置。
struct NpcConfig
{
    // 动作、对话、交互、位置、Spine、文本和层级参数。
    std::string actionName;
    int32_t actionTimes = 0;
    std::vector<int32_t> chatText;
    std::vector<int32_t> closeSystemPx;
    int32_t goSpine = 0;
    std::vector<int32_t> goSpinePx;
    int32_t id            = 0;
    int32_t nameId        = 0;
    int32_t npcExpression = 0;
    int32_t npcType       = 0;
    int32_t radius        = 0;
    std::vector<int32_t> relativePosition;
    int32_t resSpineId    = 0;
    int32_t resSpineIdExt = 0;
    std::vector<int32_t> sayText;
    int32_t shadow = 0;
    std::vector<int32_t> spineRelativePosition;
    std::vector<int32_t> systemId;
    std::variant<int32_t, std::string> systemSpine;
    int32_t tier     = 0;
    int32_t tierExt  = 0;
    int32_t velocity = 0;
    std::vector<int32_t> vertexPos;
    std::vector<int32_t> vertexPosChat;
    std::vector<int32_t> vertexPosTalk;
    std::vector<int32_t> vertexPosTask;

    MG_REFLECT(NpcConfig,
               actionName,
               actionTimes,
               chatText,
               closeSystemPx,
               goSpine,
               goSpinePx,
               id,
               nameId,
               npcExpression,
               npcType,
               radius,
               relativePosition,
               resSpineId,
               resSpineIdExt,
               sayText,
               shadow,
               spineRelativePosition,
               systemId,
               systemSpine,
               tier,
               tierExt,
               velocity,
               vertexPos,
               vertexPosChat,
               vertexPosTalk,
               vertexPosTask)
};

// portal 表映射的传送门实体配置。
struct PortalConfig
{
    // 交互、位置、Spine、动画和层级参数。
    std::vector<int32_t> closeSystemPx;
    std::vector<int32_t> coercePx;
    int32_t goSpine = 0;
    std::vector<int32_t> goSpinePx;
    int32_t id     = 0;
    int32_t nameId = 0;
    int32_t radius = 0;
    std::vector<int32_t> relativePosition;
    int32_t resSpineId    = 0;
    int32_t resSpineIdExt = 0;
    int32_t shadow        = 0;
    std::vector<std::vector<std::variant<int32_t, std::string>>> spineAnimations;
    std::vector<int32_t> spineRelativePosition;
    int32_t tier     = 0;
    int32_t tierExt  = 0;
    int32_t velocity = 0;

    MG_REFLECT(PortalConfig,
               closeSystemPx,
               coercePx,
               goSpine,
               goSpinePx,
               id,
               nameId,
               radius,
               relativePosition,
               resSpineId,
               resSpineIdExt,
               shadow,
               spineAnimations,
               spineRelativePosition,
               tier,
               tierExt,
               velocity)
};

// res_spine 表映射的 Spine 资源配置。
struct ResSpineConfig
{
    // 二进制标记、文件名、标识和缩放。
    int32_t binary = 0;
    std::string fileName;
    int32_t id = 0;
    Fixed scale;

    MG_REFLECT(ResSpineConfig, binary, fileName, id, scale)
};
}  // namespace mugen::conf
