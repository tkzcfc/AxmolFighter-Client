#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// room 表映射的战斗房间配置。
struct RoomConfig
{
    // 角色出生位置、朝向、状态与战斗规则参数。
    std::vector<int32_t> actorBuffIds, actorPosX, actorPosZ, actorVectorX, battleRuleParam;
    int32_t battleRuleType = 0, explore = 0;
    // 物品出生数据、图标和房间标识。
    std::vector<int32_t> goodIds;
    std::vector<std::vector<int32_t>> goodPos;
    std::vector<int32_t> goodVectorX;
    int32_t iconGo = 0, id = 0;
    // 警告、地图、小地图与怪物难度参数。
    std::vector<int32_t> isWarning;
    std::string mapKey;
    int32_t minimapSlot = 0, monsterAiDifficulty = 0;
    // 怪物掉落、血条、类型、等级、位置和朝向。
    std::vector<std::vector<int32_t>> monsterDropGoods;
    std::vector<int32_t> monsterHpBar, monsterIds, monsterLevels, monsterPosX, monsterPosZ, monsterVectorX;
    // 房间名称和障碍物出生数据。
    int32_t nameId = 0;
    std::vector<int32_t> obstacleIds, obstacleLevel;
    std::vector<std::vector<int32_t>> obstaclePos;
    std::vector<int32_t> obstacleVectorX;
    // 开启提示和传送门目标位置、房间、权重。
    int32_t openTips = 0;
    std::vector<std::vector<int32_t>> portalDestPosX, portalDestPosZ, portalDestRoomIds, portalDestRoomWeight;
    // 传送门目标类型、朝向、限制、位置和槽位。
    std::vector<int32_t> portalDestType;
    std::vector<std::vector<int32_t>> portalDestVectorX;
    std::vector<int32_t> portalIds, portalLimitType, portalLimitValue, portalPosX, portalPosZ, portalSlots;
    // 房间类型、场景、剧情、对话和胜利条件参数。
    int32_t roomType = 0, sceneName = 0;
    std::vector<std::vector<int32_t>> storyId;
    int32_t talkFadeTime = 0, talkRoom = 0, talkSound = 0, talkText = 0, victoryCond = 0, victoryTips = 0;

    MG_REFLECT(RoomConfig,
               actorBuffIds,
               actorPosX,
               actorPosZ,
               actorVectorX,
               battleRuleParam,
               battleRuleType,
               explore,
               goodIds,
               goodPos,
               goodVectorX,
               iconGo,
               id,
               isWarning,
               mapKey,
               minimapSlot,
               monsterAiDifficulty,
               monsterDropGoods,
               monsterHpBar,
               monsterIds,
               monsterLevels,
               monsterPosX,
               monsterPosZ,
               monsterVectorX,
               nameId,
               obstacleIds,
               obstacleLevel,
               obstaclePos,
               obstacleVectorX,
               openTips,
               portalDestPosX,
               portalDestPosZ,
               portalDestRoomIds,
               portalDestRoomWeight,
               portalDestType,
               portalDestVectorX,
               portalIds,
               portalLimitType,
               portalLimitValue,
               portalPosX,
               portalPosZ,
               portalSlots,
               roomType,
               sceneName,
               storyId,
               talkFadeTime,
               talkRoom,
               talkSound,
               talkText,
               victoryCond,
               victoryTips)
};

// stage 表映射的关卡节点配置。
struct StageConfig
{
    // 奖励、消耗、掉落、进度、地图和开放条件。
    int32_t coin = 0;
    std::vector<int32_t> containIndex;
    int32_t costStrength = 0, descId = 0;
    std::vector<int32_t> dropId, dropType;
    int32_t exp = 0, exploreSchedule = 0, id = 0;
    std::vector<int32_t> indexVisible;
    std::variant<int32_t, std::string> mapCsb;
    std::string mapKey;
    int32_t nameId = 0;
    std::vector<int32_t> nextNode;
    int32_t openTime = 0, roomId = 0;
    std::vector<int32_t> sceneName;
    int32_t showHidden = 0, skillP = 0, stagePassTime = 0;
    MG_REFLECT(StageConfig,
               coin,
               containIndex,
               costStrength,
               descId,
               dropId,
               dropType,
               exp,
               exploreSchedule,
               id,
               indexVisible,
               mapCsb,
               mapKey,
               nameId,
               nextNode,
               openTime,
               roomId,
               sceneName,
               showHidden,
               skillP,
               stagePassTime)
};

// copy 表映射的副本配置。
struct CopyConfig
{
    // 难度、掉落、图片、复活、地图、次数和解锁条件。
    int32_t addLevel = 0, autoCopyType = 0;
    std::vector<int32_t> bestDrop;
    std::string copyBossImage, copyImage;
    int32_t copyType = 0, id = 0;
    std::vector<int32_t> itemRevive, itemReviveCost;
    int32_t keyHud = 0;
    std::string mapCsb;
    int32_t mapNameId = 0, maxEnter = 0, maxRevive = 0, nameId = 0, recommendFighting = 0, stageId = 0;
    std::vector<int32_t> starTaskId, unlockType, unlockValue;
    MG_REFLECT(CopyConfig,
               addLevel,
               autoCopyType,
               bestDrop,
               copyBossImage,
               copyImage,
               copyType,
               id,
               itemRevive,
               itemReviveCost,
               keyHud,
               mapCsb,
               mapNameId,
               maxEnter,
               maxRevive,
               nameId,
               recommendFighting,
               stageId,
               starTaskId,
               unlockType,
               unlockValue)
};

// chapter 表映射的章节配置。
struct ChapterConfig
{
    // 章节副本、主线奖励、地图偏移和阶段奖励。
    int32_t actId = 0;
    std::vector<int32_t> copys;
    int32_t id = 0, isHide = 0, mainCopyTemplate = 0;
    std::vector<int32_t> mainCopys;
    int32_t mainRewardBestShow = 0;
    std::vector<std::vector<int32_t>> mainRewardCount;
    std::vector<int32_t> mainRewardGoal;
    std::vector<std::vector<int32_t>> mainRewardId;
    int32_t mapOffsetX = 0, mapOffsetY = 0, nameId = 0, rewardBestShow = 0;
    std::vector<std::vector<int32_t>> rewardPhases;
    MG_REFLECT(ChapterConfig,
               actId,
               copys,
               id,
               isHide,
               mainCopyTemplate,
               mainCopys,
               mainRewardBestShow,
               mainRewardCount,
               mainRewardGoal,
               mainRewardId,
               mapOffsetX,
               mapOffsetY,
               nameId,
               rewardBestShow,
               rewardPhases)
};

// camp 表映射的营地配置。
struct CampConfig
{
    // 角色、物品、NPC、障碍和传送门的地图摆放数据。
    std::vector<int32_t> actorPosX;
    std::vector<Fixed> actorPosZ;
    std::vector<int32_t> actorVectorX, connectCity;
    int32_t containIndex = 0;
    std::vector<int32_t> goodIds;
    std::vector<std::vector<int32_t>> goodPos;
    std::vector<int32_t> goodVectorX;
    int32_t id = 0;
    std::string mapKey;
    int32_t minimapSlot = 0, nameId = 0;
    std::vector<int32_t> npcIds, npcPosX, npcPosZ, npcSlots, obstacleIds;
    std::vector<std::vector<int32_t>> obstaclePos;
    std::vector<int32_t> obstacleVectorX;
    std::vector<std::vector<int32_t>> portalDestPosX, portalDestPosZ, portalDestRoomIds, portalDestRoomWeight;
    std::vector<int32_t> portalDestType;
    std::vector<std::vector<int32_t>> portalDestVectorX;
    std::vector<int32_t> portalIds, portalLimitType, portalLimitValue, portalPosX, portalPosZ, portalSlots, sceneName;
    std::vector<std::vector<int32_t>> storyId;
    MG_REFLECT(CampConfig,
               actorPosX,
               actorPosZ,
               actorVectorX,
               connectCity,
               containIndex,
               goodIds,
               goodPos,
               goodVectorX,
               id,
               mapKey,
               minimapSlot,
               nameId,
               npcIds,
               npcPosX,
               npcPosZ,
               npcSlots,
               obstacleIds,
               obstaclePos,
               obstacleVectorX,
               portalDestPosX,
               portalDestPosZ,
               portalDestRoomIds,
               portalDestRoomWeight,
               portalDestType,
               portalDestVectorX,
               portalIds,
               portalLimitType,
               portalLimitValue,
               portalPosX,
               portalPosZ,
               portalSlots,
               sceneName,
               storyId)
};

// town 表映射的城镇配置。
struct TownConfig
{
    // 角色、物品、NPC、障碍、传送门和展示条件数据。
    int32_t actorPosX = 0, actorPosZ = 0, actorVectorX = 0;
    std::vector<int32_t> cityShowLimit;
    std::vector<std::variant<int32_t, std::string>> extraUi;
    std::vector<int32_t> goodIds;
    std::vector<std::vector<int32_t>> goodPos;
    std::vector<int32_t> goodVectorX;
    int32_t id = 0;
    std::string mapKey;
    int32_t nameId = 0;
    std::vector<int32_t> npcIds, npcPosX, npcPosZ, npcSlots, obstacleIds;
    std::vector<std::vector<int32_t>> obstaclePos;
    std::vector<int32_t> obstacleVectorX;
    std::vector<std::vector<int32_t>> portalDestPosX, portalDestPosZ, portalDestRealRoomIds, portalDestRoomIds,
        portalDestRoomWeight;
    std::vector<int32_t> portalDestType;
    std::vector<std::vector<int32_t>> portalDestVectorX;
    std::vector<int32_t> portalIds, portalLimitType, portalLimitValue, portalPosX, portalPosZ, portalSlots, sceneName;
    int32_t showNum = 0;
    std::vector<int32_t> showType;
    std::vector<std::vector<int32_t>> showWhere, storyId;
    MG_REFLECT(TownConfig,
               actorPosX,
               actorPosZ,
               actorVectorX,
               cityShowLimit,
               extraUi,
               goodIds,
               goodPos,
               goodVectorX,
               id,
               mapKey,
               nameId,
               npcIds,
               npcPosX,
               npcPosZ,
               npcSlots,
               obstacleIds,
               obstaclePos,
               obstacleVectorX,
               portalDestPosX,
               portalDestPosZ,
               portalDestRealRoomIds,
               portalDestRoomIds,
               portalDestRoomWeight,
               portalDestType,
               portalDestVectorX,
               portalIds,
               portalLimitType,
               portalLimitValue,
               portalPosX,
               portalPosZ,
               portalSlots,
               sceneName,
               showNum,
               showType,
               showWhere,
               storyId)
};
}  // namespace mugen::conf
