#pragma once

#include "mugen/conf/GameDef.h"
#include "mugen/core/math/Vec2.h"
#include "mugen/core/math/Vec3.h"

NS_MG_BEGIN

// 传送目的地（权重候选之一）
class PortalDestEntry : public Object
{
public:
    typedef Object Super;

public:
    PortalDestEntry() {}

    virtual ~PortalDestEntry() {}

    // 目的地 X
    int32_t posX = 0;

    // 目的地 Z
    int32_t posZ = 0;

    // 目标房间/城 id
    int32_t roomId = 0;

    // 实际房间 id（城镇用）
    int32_t realRoomId = 0;

    // 权重
    int32_t weight = 0;

    // 面向
    int32_t vectorX = 0;

    MG_DEFINE_SERIALIZABLE(posX, posZ, roomId, realRoomId, weight, vectorX);
};

// 传送门槽位
class PortalEntry : public Object
{
public:
    typedef Object Super;

public:
    PortalEntry() {}

    virtual ~PortalEntry() {}

    // 传送门实体 id
    int32_t portalId = 0;

    // 场景槽位
    int32_t slot = 0;

    // 门在地图中的坐标（由场景 POR_slot 回填，table 权威）
    int32_t posX = 0;
    int32_t posZ = 0;

    // 目的地类型
    int32_t destType = 0;

    // 限制类型
    int32_t limitType = 0;

    // 限制值
    int32_t limitValue = 0;

    // 目的地候选
    std::vector<PortalDestEntry> dests;

    MG_DEFINE_SERIALIZABLE(portalId, slot, posX, posZ, destType, limitType, limitValue, dests);
};

// NPC 槽位
class NpcSlotEntry : public Object
{
public:
    typedef Object Super;

public:
    NpcSlotEntry() {}

    virtual ~NpcSlotEntry() {}

    // NPC 配置 id
    int32_t npcId = 0;

    // 场景槽位
    int32_t slot = 0;

    // NPC 在地图中的坐标（由场景 NPC_slot 回填，table 权威）
    int32_t posX = 0;
    int32_t posZ = 0;

    MG_DEFINE_SERIALIZABLE(npcId, slot, posX, posZ);
};

// 掉落物/交互物
class GoodEntry : public Object
{
public:
    typedef Object Super;

public:
    GoodEntry() {}

    virtual ~GoodEntry() {}

    // 物品/实体 id
    int32_t goodId = 0;

    // 位置 X
    int32_t posX = 0;

    // 位置 Z
    int32_t posZ = 0;

    // 面向
    int32_t vectorX = 0;

    MG_DEFINE_SERIALIZABLE(goodId, posX, posZ, vectorX);
};

// 障碍物
class ObstacleEntry : public Object
{
public:
    typedef Object Super;

public:
    ObstacleEntry() {}

    virtual ~ObstacleEntry() {}

    // 障碍 id
    int32_t obstacleId = 0;

    // 等级
    int32_t level = 0;

    // 位置 X
    int32_t posX = 0;

    // 位置 Z
    int32_t posZ = 0;

    // 面向
    int32_t vectorX = 0;

    MG_DEFINE_SERIALIZABLE(obstacleId, level, posX, posZ, vectorX);
};

// 出生点
class ActorSpawnEntry : public Object
{
public:
    typedef Object Super;

public:
    ActorSpawnEntry() {}

    virtual ~ActorSpawnEntry() {}

    // 位置 X
    int32_t posX = 0;

    // 位置 Z
    int32_t posZ = 0;

    // 面向
    int32_t vectorX = 0;

    // 出生 buff
    int32_t buffId = 0;

    MG_DEFINE_SERIALIZABLE(posX, posZ, vectorX, buffId);
};

// 怪物投放
class MonsterEntry : public Object
{
public:
    typedef Object Super;

public:
    MonsterEntry() {}

    virtual ~MonsterEntry() {}

    // 怪物 id
    int32_t monsterId = 0;

    // 等级
    int32_t level = 0;

    // 位置 X
    int32_t posX = 0;

    // 位置 Z
    int32_t posZ = 0;

    // 面向
    int32_t vectorX = 0;

    // 血条显示
    int32_t hpBar = 0;

    // 是否警告
    int32_t isWarning = 0;

    MG_DEFINE_SERIALIZABLE(monsterId, level, posX, posZ, vectorX, hpBar, isWarning);
};

// 营地连接城镇（固定 3 个槽）
class ConnectCity : public Object
{
public:
    typedef Object Super;

public:
    ConnectCity() {}

    virtual ~ConnectCity() {}

    // 连接城镇 id 0
    int32_t city0 = 0;

    // 连接城镇 id 1
    int32_t city1 = 0;

    // 连接城镇 id 2
    int32_t city2 = 0;

    MG_DEFINE_SERIALIZABLE(city0, city1, city2);
};

// 章节奖励阶段
class ChapterRewardPhase : public Object
{
public:
    typedef Object Super;

public:
    ChapterRewardPhase() {}

    virtual ~ChapterRewardPhase() {}

    // 目标星数/进度
    int32_t goal = 0;

    // 奖励 id
    int32_t rewardId = 0;

    // 奖励数量
    int32_t rewardCount = 0;

    MG_DEFINE_SERIALIZABLE(goal, rewardId, rewardCount);
};

// 地图元数据配置（原 map_data）
class MapDataConfig : public Object
{
public:
    typedef Object Super;

public:
    MapDataConfig() {}

    virtual ~MapDataConfig() {}

    // map_data id
    int32_t id = 0;

    // 场景 key
    std::string mapKey;

    // BGM/音效 id
    int32_t soundId = 0;

    // 远景视差
    Vector2f distantOffset;

    // 中景视差
    Vector2f middleOffset;

    // 近景视差
    Vector2f nearbyOffset;

    // 遮罩视差
    Vector2f caseOffset;

    // 灯光视差
    Vector2f lightOffset;

    MG_DEFINE_SERIALIZABLE(id, mapKey, soundId, distantOffset, middleOffset, nearbyOffset, caseOffset, lightOffset);
};

// 城镇配置
class TownConfig : public Object
{
public:
    typedef Object Super;

public:
    TownConfig() {}

    virtual ~TownConfig() {}

    // 城镇 id
    int32_t id = 0;

    // 场景 mapKey
    std::string mapKey;

    // map_data id
    int32_t mapDataId = 0;

    // 名称文本 id
    int32_t nameId = 0;

    // 出生 X
    int32_t actorPosX = 0;

    // 出生 Z
    int32_t actorPosZ = 0;

    // 出生面向
    int32_t actorVectorX = 0;

    // 显示人数
    int32_t showNum = 0;

    // NPC 槽位
    std::vector<NpcSlotEntry> npcs;

    // 传送门槽位
    std::vector<PortalEntry> portals;

    // 障碍
    std::vector<ObstacleEntry> obstacles;

    // 交互物
    std::vector<GoodEntry> goods;

    MG_DEFINE_SERIALIZABLE(id,
                           mapKey,
                           mapDataId,
                           nameId,
                           actorPosX,
                           actorPosZ,
                           actorVectorX,
                           showNum,
                           npcs,
                           portals,
                           obstacles,
                           goods);
};

// 营地配置
class CampConfig : public Object
{
public:
    typedef Object Super;

public:
    CampConfig() {}

    virtual ~CampConfig() {}

    // 营地 id
    int32_t id = 0;

    // 场景 mapKey
    std::string mapKey;

    // map_data id
    int32_t mapDataId = 0;

    // 名称文本 id
    int32_t nameId = 0;

    // 小地图槽
    int32_t minimapSlot = 0;

    // containIndex
    int32_t containIndex = 0;

    // 出生点列表
    std::vector<ActorSpawnEntry> actorSpawns;

    // 连接城镇
    ConnectCity connectCity;

    // NPC 槽位
    std::vector<NpcSlotEntry> npcs;

    // 传送门槽位
    std::vector<PortalEntry> portals;

    // 障碍
    std::vector<ObstacleEntry> obstacles;

    // 交互物
    std::vector<GoodEntry> goods;

    MG_DEFINE_SERIALIZABLE(id,
                           mapKey,
                           mapDataId,
                           nameId,
                           minimapSlot,
                           containIndex,
                           actorSpawns,
                           connectCity,
                           npcs,
                           portals,
                           obstacles,
                           goods);
};

// 副本节点配置
class StageConfig : public Object
{
public:
    typedef Object Super;

public:
    StageConfig() {}

    virtual ~StageConfig() {}

    // stage id
    int32_t id = 0;

    // 入口房间 id
    int32_t roomId = 0;

    // 入口场景 mapKey
    std::string mapKey;

    // 名称文本 id
    int32_t nameId = 0;

    // 描述文本 id
    int32_t descId = 0;

    // 体力消耗
    int32_t costStrength = 0;

    // 经验
    int32_t exp = 0;

    // 金币
    int32_t coin = 0;

    // 通关时限
    int32_t stagePassTime = 0;

    // 探索格数
    int32_t exploreSchedule = 0;

    // 开放时间
    int32_t openTime = 0;

    // skill_p
    int32_t skillP = 0;

    // 下一节点索引
    std::vector<int32_t> nextNode;

    // 包含索引
    std::vector<int32_t> containIndex;

    // 索引可见
    std::vector<int32_t> indexVisible;

    // 掉落 id
    std::vector<int32_t> dropId;

    // 掉落类型
    std::vector<int32_t> dropType;

    // 场景名文本
    std::vector<int32_t> sceneName;

    MG_DEFINE_SERIALIZABLE(id,
                           roomId,
                           mapKey,
                           nameId,
                           descId,
                           costStrength,
                           exp,
                           coin,
                           stagePassTime,
                           exploreSchedule,
                           openTime,
                           skillP,
                           nextNode,
                           containIndex,
                           indexVisible,
                           dropId,
                           dropType,
                           sceneName);
};

// 主线关卡配置
class CopyConfig : public Object
{
public:
    typedef Object Super;

public:
    CopyConfig() {}

    virtual ~CopyConfig() {}

    // 关卡 id
    int32_t id = 0;

    // 关联 stage id
    int32_t stageId = 0;

    // 名称文本 id
    int32_t nameId = 0;

    // 地图名文本 id
    int32_t mapNameId = 0;

    // 副本类型
    int32_t copyType = 0;

    // 推荐战力
    int32_t recommendFighting = 0;

    // 等级修正
    int32_t addLevel = 0;

    // 最大进入次数（0 不限）
    int32_t maxEnter = 0;

    // 最大复活次数
    int32_t maxRevive = 0;

    // 钥匙 HUD
    int32_t keyHud = 0;

    // 选关图
    std::string copyImage;

    // Boss 图
    std::string copyBossImage;

    // 小地图 csb
    std::string mapCsb;

    // 最佳掉落展示
    std::vector<int32_t> bestDrop;

    // 复活道具
    std::vector<int32_t> itemRevive;

    // 复活消耗
    std::vector<int32_t> itemReviveCost;

    // 星级任务
    std::vector<int32_t> starTaskId;

    // 解锁类型
    std::vector<int32_t> unlockType;

    // 解锁值
    std::vector<int32_t> unlockValue;

    MG_DEFINE_SERIALIZABLE(id,
                           stageId,
                           nameId,
                           mapNameId,
                           copyType,
                           recommendFighting,
                           addLevel,
                           maxEnter,
                           maxRevive,
                           keyHud,
                           copyImage,
                           copyBossImage,
                           mapCsb,
                           bestDrop,
                           itemRevive,
                           itemReviveCost,
                           starTaskId,
                           unlockType,
                           unlockValue);
};

// 章节配置
class ChapterConfig : public Object
{
public:
    typedef Object Super;

public:
    ChapterConfig() {}

    virtual ~ChapterConfig() {}

    // 章节 id
    int32_t id = 0;

    // 活动 id
    int32_t actId = 0;

    // 名称文本 id
    int32_t nameId = 0;

    // 是否隐藏
    int32_t isHide = 0;

    // 主线模板
    int32_t mainCopyTemplate = 0;

    // 通关奖励数量
    int32_t mainPassRewardCount = 0;

    // 通关奖励 id
    int32_t mainPassRewardId = 0;

    // 奖励最佳展示
    int32_t mainRewardBestShow = 0;

    // 地图偏移 X
    int32_t mapOffsetX = 0;

    // 地图偏移 Y
    int32_t mapOffsetY = 0;

    // 奖励最佳展示（探索）
    int32_t rewardBestShow = 0;

    // 主线关卡 id 列表
    std::vector<int32_t> mainCopys;

    // 探索副本 id 列表
    std::vector<int32_t> copys;

    // 主线阶段奖励
    std::vector<ChapterRewardPhase> mainRewardPhases;

    MG_DEFINE_SERIALIZABLE(id,
                           actId,
                           nameId,
                           isHide,
                           mainCopyTemplate,
                           mainPassRewardCount,
                           mainPassRewardId,
                           mainRewardBestShow,
                           mapOffsetX,
                           mapOffsetY,
                           rewardBestShow,
                           mainCopys,
                           copys,
                           mainRewardPhases);
};

// NPC 实体配置
class NpcConfig : public Object
{
public:
    typedef Object Super;

public:
    NpcConfig() {}

    virtual ~NpcConfig() {}

    // NPC id
    int32_t id = 0;

    // 名称文本 id
    int32_t nameId = 0;

    // NPC 类型
    int32_t npcType = 0;

    // 交互半径
    int32_t radius = 0;

    // Spine id
    int32_t resSpineId = 0;

    // 扩展 Spine id
    int32_t resSpineIdExt = 0;

    // 表情
    int32_t npcExpression = 0;

    // 动作名
    std::string actionName;

    // 动作次数
    int32_t actionTimes = 0;

    // 系统 Spine 名
    std::string systemSpine;

    // 阴影
    int32_t shadow = 0;

    // 层级
    int32_t tier = 0;

    // 扩展层级
    int32_t tierExt = 0;

    // 速度
    int32_t velocity = 0;

    // goSpine
    int32_t goSpine = 0;

    // 相对位置
    Vector3i relativePosition;

    // Spine 相对位置
    Vector3i spineRelativePosition;

    // 头顶位置
    Vector2i vertexPos;

    // 对话位置
    Vector2i vertexPosTalk;

    // 任务位置
    Vector2i vertexPosTask;

    // 聊天位置
    Vector3i vertexPosChat;

    // 关闭系统按钮偏移
    Vector2i closeSystemPx;

    // goSpine 偏移
    Vector2i goSpinePx;

    // 系统入口 id
    std::vector<int32_t> systemId;

    // 闲聊文本
    std::vector<int32_t> chatText;

    // 说话文本
    std::vector<int32_t> sayText;

    MG_DEFINE_SERIALIZABLE(id,
                           nameId,
                           npcType,
                           radius,
                           resSpineId,
                           resSpineIdExt,
                           npcExpression,
                           actionName,
                           actionTimes,
                           systemSpine,
                           shadow,
                           tier,
                           tierExt,
                           velocity,
                           goSpine,
                           relativePosition,
                           spineRelativePosition,
                           vertexPos,
                           vertexPosTalk,
                           vertexPosTask,
                           vertexPosChat,
                           closeSystemPx,
                           goSpinePx,
                           systemId,
                           chatText,
                           sayText);
};

// 传送门实体配置
class PortalConfig : public Object
{
public:
    typedef Object Super;

public:
    PortalConfig() {}

    virtual ~PortalConfig() {}

    // 传送门 id
    int32_t id = 0;

    // 名称文本 id
    int32_t nameId = 0;

    // 交互半径
    int32_t radius = 0;

    // Spine id
    int32_t resSpineId = 0;

    // 扩展 Spine id
    int32_t resSpineIdExt = 0;

    // 阴影
    int32_t shadow = 0;

    // 层级
    int32_t tier = 0;

    // 扩展层级
    int32_t tierExt = 0;

    // 速度
    int32_t velocity = 0;

    // goSpine
    int32_t goSpine = 0;

    // 相对位置
    Vector3i relativePosition;

    // Spine 相对位置
    Vector3i spineRelativePosition;

    // 关闭系统按钮偏移
    Vector2i closeSystemPx;

    // 强制偏移
    Vector2i coercePx;

    // goSpine 偏移
    Vector2i goSpinePx;

    // Spine 动画参数（每组取首值）
    std::vector<int32_t> spineAnimations;

    MG_DEFINE_SERIALIZABLE(id,
                           nameId,
                           radius,
                           resSpineId,
                           resSpineIdExt,
                           shadow,
                           tier,
                           tierExt,
                           velocity,
                           goSpine,
                           relativePosition,
                           spineRelativePosition,
                           closeSystemPx,
                           coercePx,
                           goSpinePx,
                           spineAnimations);
};

// 战斗房间配置
class RoomConfig : public Object
{
public:
    typedef Object Super;

public:
    RoomConfig() {}

    virtual ~RoomConfig() {}

    // 房间 id
    int32_t id = 0;

    // map_data id
    int32_t mapDataId = 0;

    // 场景 mapKey
    std::string mapKey;

    // 名称文本 id
    int32_t nameId = 0;

    // 房间类型
    int32_t roomType = 0;

    // 探索标记
    int32_t explore = 0;

    // 战斗规则类型
    int32_t battleRuleType = 0;

    // 小地图槽
    int32_t minimapSlot = 0;

    // iconGo
    int32_t iconGo = 0;

    // 怪物 AI 难度
    int32_t monsterAiDifficulty = 0;

    // 场景名文本
    int32_t sceneName = 0;

    // 对话相关
    int32_t talkFadeTime = 0;
    int32_t talkRoom     = 0;
    int32_t talkSound    = 0;
    int32_t talkText     = 0;

    // 胜利条件/提示
    int32_t victoryCond = 0;
    int32_t victoryTips = 0;
    int32_t openTips    = 0;

    // 战斗规则参数
    std::vector<int32_t> battleRuleParam;

    // 出生点
    std::vector<ActorSpawnEntry> actorSpawns;

    // 怪物
    std::vector<MonsterEntry> monsters;

    // 传送门
    std::vector<PortalEntry> portals;

    // 障碍
    std::vector<ObstacleEntry> obstacles;

    // 交互物
    std::vector<GoodEntry> goods;

    // 剧情 id
    std::vector<int32_t> storyId;

    MG_DEFINE_SERIALIZABLE(id,
                           mapDataId,
                           mapKey,
                           nameId,
                           roomType,
                           explore,
                           battleRuleType,
                           minimapSlot,
                           iconGo,
                           monsterAiDifficulty,
                           sceneName,
                           talkFadeTime,
                           talkRoom,
                           talkSound,
                           talkText,
                           victoryCond,
                           victoryTips,
                           openTips,
                           battleRuleParam,
                           actorSpawns,
                           monsters,
                           portals,
                           obstacles,
                           goods,
                           storyId);
};

// ========== 战斗时间轴 / 角色 ==========

// 嵌套 int 数组的一行（skill.actionIds / buff.conditionParam / skill_ai.composition 等）
class IntListRow : public Object
{
public:
    typedef Object Super;

    IntListRow() {}
    virtual ~IntListRow() {}

    std::vector<int32_t> values;

    MG_DEFINE_SERIALIZABLE(values);
};

// 角色成品属性（转换器烘培）
class RoleAttributeConfig : public Object
{
public:
    typedef Object Super;

    RoleAttributeConfig() {}
    virtual ~RoleAttributeConfig() {}

    int32_t hpMax         = 180;
    int32_t mpMax         = 140;
    float physicalAttack  = 7.5f;
    float physicalDefense = 7.5f;
    float magicAttack     = 4.5f;
    float magicDefense    = 4.5f;
    float darkResistance  = 20.0f;
    float lightResistance = -20.0f;
    float mpRegenSpeed    = 50.0f;
    float moveSpeed       = 850.0f;
    float attackSpeed     = 850.0f;
    float castSpeed       = 700.0f;
    float hitRecovery     = 0;
    float jumpSpeed       = 200.0f;
    // 扩展属性字段
    float atk        = 0;
    float def        = 0;
    float matk       = 0;
    float mdef       = 0;
    float hp         = 0;
    float crit       = 0;
    float critDamage = 0;
    float critResist = 0;
    float dodge      = 0;
    float hit        = 0;
    float baseDamage = 0;
    float mp         = 0;

    MG_DEFINE_SERIALIZABLE(hpMax,
                           mpMax,
                           physicalAttack,
                           physicalDefense,
                           magicAttack,
                           magicDefense,
                           darkResistance,
                           lightResistance,
                           mpRegenSpeed,
                           moveSpeed,
                           attackSpeed,
                           castSpeed,
                           hitRecovery,
                           jumpSpeed,
                           atk,
                           def,
                           matk,
                           mdef,
                           hp,
                           crit,
                           critDamage,
                           critResist,
                           dodge,
                           hit,
                           baseDamage,
                           mp);
};

// 行为分支一条（状态位条件 + 行为类型 + 动画）
class BehaviorBranchConfig : public Object
{
public:
    typedef Object Super;

    BehaviorBranchConfig() {}
    virtual ~BehaviorBranchConfig() {}

    // 需要全部置位的状态位
    uint32_t requireTags = 0;
    // 禁止出现的状态位
    uint32_t denyTags = 0;
    // 行为类型
    int32_t kind = static_cast<int32_t>(BehaviorKind::kIdle);
    // 默认动画名（Spine）
    std::string animation;
    // 是否循环
    bool loop = true;

    MG_DEFINE_SERIALIZABLE(requireTags, denyTags, kind, animation, loop);
};

// 行为模板（转换器按 roleType 烘培）
class BehaviorTemplateConfig : public Object
{
public:
    typedef Object Super;

    BehaviorTemplateConfig() {}
    virtual ~BehaviorTemplateConfig() {}

    int32_t id = 0;
    // 有序优先级分支（前高后低）
    std::vector<BehaviorBranchConfig> branches;

    MG_DEFINE_SERIALIZABLE(id, branches);
};

// 动作攻击段
class ActionAttackConfig : public Object
{
public:
    typedef Object Super;

public:
    ActionAttackConfig() {}

    virtual ~ActionAttackConfig() {}

    int32_t id                  = 0;
    int32_t action              = 0;
    float actionScaleTime       = 1.0f;
    int32_t loop                = 0;
    int32_t interruptFrame      = -1;
    int32_t interruptExtraFrame = -1;
    std::vector<int32_t> effectIds;
    std::vector<int32_t> effectFrames;
    std::vector<int32_t> displaySpineIds;
    int32_t displaySpineFrame = 0;
    int32_t displacementId    = -1;
    int32_t control           = 0;
    float controlVelocity     = 0.0f;
    std::vector<int32_t> soundId;
    std::vector<int32_t> buffIds;
    int32_t cameraFrame     = -1;
    int32_t cameraId        = -1;
    int32_t actionDelayTime = 0;
    // 扩展：定位 / 朝向 / 变身 / 定身 / 残影
    int32_t actionPosType = -1;
    Vector3f relativeActionPos;
    int32_t actionOrientation = -1;
    int32_t transformFrame    = -1;
    int32_t transformId       = -1;
    int32_t transformType     = -1;
    int32_t ghost             = -1;
    int32_t staticTarget      = -1;
    int32_t staticStartFrame  = -1;
    int32_t staticTime        = 0;
    int32_t staticResetTime   = 0;
    // UI / 障碍 / 落地 / 阴影（action_attack）
    int32_t dialogShow = -1;
    int32_t nameShow   = 0;
    int32_t tipsShow   = -1;
    int32_t obstruct   = 0;
    int32_t floor      = -1;
    float shadow       = 0.0f;

    MG_DEFINE_SERIALIZABLE(id,
                           action,
                           actionScaleTime,
                           loop,
                           interruptFrame,
                           interruptExtraFrame,
                           effectIds,
                           effectFrames,
                           displaySpineIds,
                           displaySpineFrame,
                           displacementId,
                           control,
                           controlVelocity,
                           soundId,
                           buffIds,
                           cameraFrame,
                           cameraId,
                           actionDelayTime,
                           actionPosType,
                           relativeActionPos,
                           actionOrientation,
                           transformFrame,
                           transformId,
                           transformType,
                           ghost,
                           staticTarget,
                           staticStartFrame,
                           staticTime,
                           staticResetTime,
                           dialogShow,
                           nameShow,
                           tipsShow,
                           obstruct,
                           floor,
                           shadow);
};

// 技能攻击配置
class SkillAttackConfig : public Object
{
public:
    typedef Object Super;

public:
    SkillAttackConfig() {}

    virtual ~SkillAttackConfig() {}

    int32_t id = 0;
    // 完整二维动作表 [toward][段]（action_ids）
    std::vector<IntListRow> actionIds;
    // 兼容：第一组扁平（可由 actionIds[0] 派生）
    std::vector<int32_t> primaryActionIds;
    int32_t nextSkill = -1;
    int32_t cd        = 0;
    int32_t mp        = 0;
    int32_t ep        = 0;
    int32_t icon      = 0;
    int32_t nameId    = 0;
    int32_t descId    = 0;
    int32_t sorder    = 0;
    // 扩展
    int32_t cdCount = -1;
    int32_t crystal = 0;
    int32_t type    = -1;
    std::vector<int32_t> sorderControlType;

    MG_DEFINE_SERIALIZABLE(id,
                           actionIds,
                           primaryActionIds,
                           nextSkill,
                           cd,
                           mp,
                           ep,
                           icon,
                           nameId,
                           descId,
                           sorder,
                           cdCount,
                           crystal,
                           type,
                           sorderControlType);
};

// 受击配置
class SkillHitTableConfig : public Object
{
public:
    typedef Object Super;

public:
    SkillHitTableConfig() {}

    virtual ~SkillHitTableConfig() {}

    int32_t id                      = 0;
    int32_t displacementId          = -1;
    int32_t airDisplacementId       = -1;
    int32_t floorDisplacementId     = -1;
    int32_t freezeTime              = 0;
    int32_t freezeTimeDelay         = 0;
    int32_t freezeTimeControlEffect = 0;
    int32_t freezeTimeControlRole   = 0;
    int32_t hitCondition            = 0;
    int32_t hitCounts               = -1;
    int32_t hitInterval             = -1;
    int32_t hitMust                 = 0;
    int32_t hitRigidity             = 0;
    int32_t hitType                 = 0;
    float hurtRate                  = 1.0f;
    int32_t hurtType                = 0;
    int32_t stiffTime               = 0;

    MG_DEFINE_SERIALIZABLE(id,
                           displacementId,
                           airDisplacementId,
                           floorDisplacementId,
                           freezeTime,
                           freezeTimeDelay,
                           freezeTimeControlEffect,
                           freezeTimeControlRole,
                           hitCondition,
                           hitCounts,
                           hitInterval,
                           hitMust,
                           hitRigidity,
                           hitType,
                           hurtRate,
                           hurtType,
                           stiffTime);
};

// 属性模板
class AttributeTemplateConfig : public Object
{
public:
    typedef Object Super;

public:
    AttributeTemplateConfig() {}

    virtual ~AttributeTemplateConfig() {}

    int32_t id             = 0;
    float atk              = 0;
    float def              = 0;
    float matk             = 0;
    float mdef             = 0;
    float hp               = 0;
    float crit             = 0;
    float critDamage       = 0;
    float critDamageResist = 0;
    float critResist       = 0;
    float dodge            = 0;
    float hit              = 0;
    float baseDamage       = 0;
    float agility          = 0;
    float habitus          = 0;
    float spirit           = 0;
    float sourceForce      = 0;

    MG_DEFINE_SERIALIZABLE(id,
                           atk,
                           def,
                           matk,
                           mdef,
                           hp,
                           crit,
                           critDamage,
                           critDamageResist,
                           critResist,
                           dodge,
                           hit,
                           baseDamage,
                           agility,
                           habitus,
                           spirit,
                           sourceForce);
};

// Spine 资源
class ResSpineConfig : public Object
{
public:
    typedef Object Super;

public:
    ResSpineConfig() {}

    virtual ~ResSpineConfig() {}

    int32_t id = 0;
    std::string spine;
    float scale = 0.0f;
    // 转换器烘培
    std::string atlas;
    std::string defaultSkin;

    MG_DEFINE_SERIALIZABLE(id, spine, scale, atlas, defaultSkin);
};

// 位移表
class DisplacementConfig : public Object
{
public:
    typedef Object Super;

    DisplacementConfig() {}
    virtual ~DisplacementConfig() {}

    int32_t id = 0;
    Vector3f velocity;
    Vector3i velocityTime;
    Vector3f acceleration;
    Vector3i accelerationTime;
    float gravity   = 0;
    int32_t bounces = 0;

    MG_DEFINE_SERIALIZABLE(id, velocity, velocityTime, acceleration, accelerationTime, gravity, bounces);
};

// 镜头表
class CameraConfig : public Object
{
public:
    typedef Object Super;

    CameraConfig() {}
    virtual ~CameraConfig() {}

    int32_t id         = 0;
    float amplitude    = 0;
    float amplitudeX   = 0;
    float amplitudeY   = 0;
    float duration     = 0;
    int32_t freezeTime = 0;
    int32_t times      = 1;
    int32_t level      = 0;
    std::string modifier;

    MG_DEFINE_SERIALIZABLE(id, amplitude, amplitudeX, amplitudeY, duration, freezeTime, times, level, modifier);
};

// 特效实体
class EffectConfig : public Object
{
public:
    typedef Object Super;

    EffectConfig() {}
    virtual ~EffectConfig() {}

    int32_t id           = 0;
    int32_t resSpineId   = 0;
    int32_t positionType = -1;
    Vector3f relativePosition;
    int32_t follow            = 0;
    int32_t autoRelease       = 0;
    int32_t effectOrientation = -1;
    int32_t skillHitId        = -1;
    float radius              = 20.0f;
    // 命中链路 / 表现（entity_effect）
    std::vector<int32_t> actionIds;
    std::vector<int32_t> buffId;
    std::vector<int32_t> debuffId;
    int32_t buffAllId = -1;
    int32_t collision = 0;
    int32_t comboExp  = 0;
    int32_t control   = 0;
    int32_t effectType = -1;
    int32_t energy     = 0;
    std::vector<int32_t> hitEffectIds;
    std::vector<int32_t> hitExtraControl;
    int32_t hitTarget     = -1;
    int32_t nextEffectId  = -1;
    int32_t preloadCount  = 0;
    int32_t shadow        = 0;
    int32_t tier          = 0;
    float velocity        = 0.0f;
    Vector3f spineRelativePosition;
    // 特殊能力预留（Phase 2）
    int32_t specialabilityAllId = -1;
    int32_t specialabilityId    = -1;

    MG_DEFINE_SERIALIZABLE(id,
                           resSpineId,
                           positionType,
                           relativePosition,
                           follow,
                           autoRelease,
                           effectOrientation,
                           skillHitId,
                           radius,
                           actionIds,
                           buffId,
                           debuffId,
                           buffAllId,
                           collision,
                           comboExp,
                           control,
                           effectType,
                           energy,
                           hitEffectIds,
                           hitExtraControl,
                           hitTarget,
                           nextEffectId,
                           preloadCount,
                           shadow,
                           tier,
                           velocity,
                           spineRelativePosition,
                           specialabilityAllId,
                           specialabilityId);
};

// Buff 规则（buff_rule → className）
class BuffRuleConfig : public Object
{
public:
    typedef Object Super;

    BuffRuleConfig() {}
    virtual ~BuffRuleConfig() {}

    int32_t id             = 0;
    int32_t buffType       = 0;
    std::string className;
    int32_t fashionShowText = 0;

    MG_DEFINE_SERIALIZABLE(id, buffType, className, fashionShowText);
};

// Buff（buff_base 全量）
class BuffConfig : public Object
{
public:
    typedef Object Super;

    BuffConfig() {}
    virtual ~BuffConfig() {}

    int32_t id       = 0;
    int32_t ruleId   = 0;
    int32_t interval = 0;
    int32_t times    = 0;
    std::vector<float> paramValue;
    std::string className;
    int32_t addType = 0;
    std::vector<int32_t> artifactSkillIds;
    int32_t audioId              = -1;
    int32_t began                = -1;
    int32_t bindSpecialAbilityId = -1;
    int32_t binding              = 0;
    int32_t buffType             = 0;
    int32_t cd                   = 0;
    int32_t cdPvp                = 0;
    std::vector<int32_t> condition;
    std::vector<IntListRow> conditionParam;
    int32_t descId = 0;
    int32_t ended  = -1;
    std::vector<int32_t> eventParam;
    int32_t executeType = 0;
    int32_t hurtType    = 0;
    int32_t icon        = -1;
    int32_t iconDescId  = 0;
    int32_t inherit     = 0;
    int32_t innerCd     = 0;
    int32_t nameId      = 0;
    int32_t priority    = 0;
    int32_t probability = 100;
    int32_t probabilityRepeat = 0;
    int32_t removeRepeatAll   = 0;
    int32_t repeatMax         = 1;
    int32_t resetType         = 0;
    int32_t showTips          = 0;
    int32_t spineId           = -1;
    std::vector<float> spineOffsets;
    std::vector<int32_t> spineStep;
    int32_t subType = -1;
    int32_t target  = 0;

    MG_DEFINE_SERIALIZABLE(id,
                           ruleId,
                           interval,
                           times,
                           paramValue,
                           className,
                           addType,
                           artifactSkillIds,
                           audioId,
                           began,
                           bindSpecialAbilityId,
                           binding,
                           buffType,
                           cd,
                           cdPvp,
                           condition,
                           conditionParam,
                           descId,
                           ended,
                           eventParam,
                           executeType,
                           hurtType,
                           icon,
                           iconDescId,
                           inherit,
                           innerCd,
                           nameId,
                           priority,
                           probability,
                           probabilityRepeat,
                           removeRepeatAll,
                           repeatMax,
                           resetType,
                           showTips,
                           spineId,
                           spineOffsets,
                           spineStep,
                           subType,
                           target);
};

// AI（entity_ai；skillAiIds 本阶段补齐）
class AiConfig : public Object
{
public:
    typedef Object Super;

    AiConfig() {}
    virtual ~AiConfig() {}

    int32_t id = 0;
    Vector2i chaseScopeX;
    Vector2i chaseScopeZ;
    Vector2i targetScopeX;
    Vector2i targetScopeZ;
    std::vector<int32_t> skillIds;
    int32_t skillInterval      = 0;
    int32_t skillPriorityLevel = 0;
    std::vector<int32_t> skillAiIds;
    /** 巡逻半径（出生点为心；0=运行时用 chaseScope 推导默认） */
    int32_t patrolScope = 0;

    MG_DEFINE_SERIALIZABLE(id,
                           chaseScopeX,
                           chaseScopeZ,
                           targetScopeX,
                           targetScopeZ,
                           skillIds,
                           skillInterval,
                           skillPriorityLevel,
                           skillAiIds,
                           patrolScope);
};

// 技能 AI 条件（skill_ai）
class SkillAiConfig : public Object
{
public:
    typedef Object Super;

    SkillAiConfig() {}
    virtual ~SkillAiConfig() {}

    int32_t id      = 0;
    int32_t checkCd = 0;
    std::vector<IntListRow> composition;
    int32_t loadCd   = 0;
    int32_t oppCombo = -1;
    Vector2i oppDisX;
    Vector2i oppDisZ;
    int32_t oppSkillId = -1;
    int32_t oppStatus  = -1;
    int32_t prob       = 100;
    Vector2i selfHp;
    int32_t selfStatus = -1;
    int32_t useCount   = -1;

    MG_DEFINE_SERIALIZABLE(id,
                           checkCd,
                           composition,
                           loadCd,
                           oppCombo,
                           oppDisX,
                           oppDisZ,
                           oppSkillId,
                           oppStatus,
                           prob,
                           selfHp,
                           selfStatus,
                           useCount);
};

// 伤害标准
class SkillHurtConfig : public Object
{
public:
    typedef Object Super;

    SkillHurtConfig() {}
    virtual ~SkillHurtConfig() {}

    int32_t id                 = 0;
    int32_t atkStandard        = 0;
    int32_t critStandard       = 0;
    int32_t critDamageStandard = 0;
    int32_t dodgeStandard      = 0;
    int32_t hurt               = 0;
    int32_t monsterHurt        = 0;

    MG_DEFINE_SERIALIZABLE(id, atkStandard, critStandard, critDamageStandard, dodgeStandard, hurt, monsterHurt);
};

// 触发规则 overlay（手工；按技能 id 键控）
class SkillActivationOverlayConfig : public Object
{
public:
    typedef Object Super;

    SkillActivationOverlayConfig() {}
    virtual ~SkillActivationOverlayConfig() {}

    int32_t skillId                 = 0;
    uint32_t slotTriggerFlags       = 0;
    uint32_t allowTags              = 0;
    uint32_t denyTags               = 0;
    int32_t comboWindowMs           = 500;
    uint32_t inputBufferReleaseTags = 0;
    int32_t inputBufferTimeoutMs    = 500;
    std::vector<int32_t> comboInputs;

    MG_DEFINE_SERIALIZABLE(skillId,
                           slotTriggerFlags,
                           allowTags,
                           denyTags,
                           comboWindowMs,
                           inputBufferReleaseTags,
                           inputBufferTimeoutMs,
                           comboInputs);
};

// 角色实体
class RoleConfig : public Object
{
public:
    typedef Object Super;

public:
    RoleConfig() {}

    virtual ~RoleConfig() {}

    int32_t id         = 0;
    int32_t roleType   = 0;
    int32_t resSpineId = 0;
    std::vector<int32_t> aiIds;
    std::vector<float> attributeRate;
    std::vector<int32_t> buffIds;
    int32_t nameId = 0;
    std::string headImage;
    float velocity   = 0.0f;
    int32_t rigidity = 0;
    std::vector<int32_t> defaultSkillIds;
    // 角色运行时属性
    int32_t radius       = 20;
    int32_t monsterCamps = 0;
    std::vector<int32_t> soundId;
    int32_t soundType           = 0;
    int32_t deathDisplacementId = -1;
    int32_t deathEffectId       = -1;
    int32_t hitDisplacementId   = -1;
    int32_t hitStiffTime        = -1;
    int32_t hpBarCount          = 1;
    int32_t shadow              = 0;
    int32_t tier                = 0;
    Vector2i vertexPos;
    Vector2i size{40, 40};
    int32_t behaviorTemplateId = 1;
    // 转换器烘培成品属性
    RoleAttributeConfig attribute;

    MG_DEFINE_SERIALIZABLE(id,
                           roleType,
                           resSpineId,
                           aiIds,
                           attributeRate,
                           buffIds,
                           nameId,
                           headImage,
                           velocity,
                           rigidity,
                           defaultSkillIds,
                           radius,
                           monsterCamps,
                           soundId,
                           soundType,
                           deathDisplacementId,
                           deathEffectId,
                           hitDisplacementId,
                           hitStiffTime,
                           hpBarCount,
                           shadow,
                           tier,
                           vertexPos,
                           size,
                           behaviorTemplateId,
                           attribute);
};

// 音效资源
class ResSoundConfig : public Object
{
public:
    typedef Object Super;

public:
    ResSoundConfig() {}

    virtual ~ResSoundConfig() {}

    // 音效 id
    int32_t id = 0;

    // Content 相对路径（如 mugen/sound/xxx.mp3）
    std::string fileName;

    // 是否循环（源表为 0/1）
    int32_t loop = 0;

    // 音量 0~1
    float volume = 1.0f;

    MG_DEFINE_SERIALIZABLE(id, fileName, loop, volume);
};

// UI 界面音效映射
class SoundUiConfig : public Object
{
public:
    typedef Object Super;

public:
    SoundUiConfig() {}

    virtual ~SoundUiConfig() {}

    std::string viewName;

    std::vector<std::string> buttonName;

    std::vector<int32_t> soundIdButton;

    int32_t soundIdOp = -1;

    int32_t soundIdEd = -1;

    MG_DEFINE_SERIALIZABLE(viewName, buttonName, soundIdButton, soundIdOp, soundIdEd);
};

// Spine 动作音效映射
class SoundSpineConfig : public Object
{
public:
    typedef Object Super;

public:
    SoundSpineConfig() {}

    virtual ~SoundSpineConfig() {}

    int32_t id = 0;

    int32_t spineId = 0;

    std::vector<std::string> actionIds;

    std::vector<int32_t> soundIds;

    MG_DEFINE_SERIALIZABLE(id, spineId, actionIds, soundIds);
};

// Spine BGM 映射
class SoundSpineBgmConfig : public Object
{
public:
    typedef Object Super;

public:
    SoundSpineBgmConfig() {}

    virtual ~SoundSpineBgmConfig() {}

    int32_t id = 0;

    int32_t spineId = 0;

    std::vector<std::string> actionIds;

    std::vector<int32_t> soundIds;

    MG_DEFINE_SERIALIZABLE(id, spineId, actionIds, soundIds);
};

// 地图 Spine 音效
class SoundMapSpineConfig : public Object
{
public:
    typedef Object Super;

public:
    SoundMapSpineConfig() {}

    virtual ~SoundMapSpineConfig() {}

    int32_t id = 0;

    // spine 路径或占位（源可能为数字）
    std::string spineName;

    std::vector<std::string> actionIds;

    std::vector<int32_t> soundIds;

    MG_DEFINE_SERIALIZABLE(id, spineName, actionIds, soundIds);
};

// 发送消息音效
class SoundSendMessageConfig : public Object
{
public:
    typedef Object Super;

public:
    SoundSendMessageConfig() {}

    virtual ~SoundSendMessageConfig() {}

    int32_t id = 0;

    int32_t sendmessageId = 0;

    int32_t soundIdTrue = -1;

    int32_t soundIdFalse = -1;

    MG_DEFINE_SERIALIZABLE(id, sendmessageId, soundIdTrue, soundIdFalse);
};

// 对话文本一组 id
class SoundTalkTextGroup : public Object
{
public:
    typedef Object Super;

public:
    SoundTalkTextGroup() {}

    virtual ~SoundTalkTextGroup() {}

    std::vector<int32_t> ids;

    MG_DEFINE_SERIALIZABLE(ids);
};

// 对话音效
class SoundTalkConfig : public Object
{
public:
    typedef Object Super;

public:
    SoundTalkConfig() {}

    virtual ~SoundTalkConfig() {}

    int32_t id = 0;

    int32_t talkType = 0;

    std::vector<int32_t> levelLimit;

    std::vector<int32_t> fadeTime;

    std::vector<int32_t> param;

    std::vector<int32_t> position;

    std::vector<int32_t> talkSound;

    std::vector<SoundTalkTextGroup> talkText;

    MG_DEFINE_SERIALIZABLE(id, talkType, levelLimit, fadeTime, param, position, talkSound, talkText);
};

// ========== 装备表（equip/fashion/fashion_suit/item_base/res_fashion）==========

// 装备
class EquipConfig : public Object
{
public:
    typedef Object Super;

public:
    EquipConfig() {}

    virtual ~EquipConfig() {}

    int32_t id = 0;

    // 职业（occupation）
    int32_t occupation = 0;

    // 部位
    int32_t position = 0;

    // 关联物品 id（item_base 区间）
    int32_t goodsId = 0;

    // 图标
    std::string icon;

    // 名称文本 id
    int32_t nameId = 0;

    // 描述文本 id
    int32_t descId = 0;

    // 需求等级
    int32_t level = 0;

    // 最大等级
    int32_t maxLevel = 0;

    // 品质
    int32_t quality = 0;

    // 套装 id
    int32_t suitId = 0;

    // buff id
    int32_t buffId = 0;

    // 交易所 id
    int32_t bourseId = 0;

    // 芯片 id
    int32_t chipId = 0;

    // 锻造 id
    int32_t forgeId = 0;

    // 附魔类型
    int32_t enchantType = 0;

    // 时间类型
    int32_t timeType = 0;

    // 持续时间
    int32_t lastTime = 0;

    // 排序权重
    int32_t sortWeight = 0;

    // 特殊能力 id
    int32_t specialAbilityId = 0;

    // 随机属性数量
    int32_t randAttributeNum = 0;

    // 随机属性精炼 id
    int32_t randAttributeRefinedId = 0;

    // 闪率
    int32_t flickerRate = 0;

    // 升级道具 id
    int32_t updataItemId = 0;

    // 升级道具数量
    int32_t updataItemNum = 0;

    // 升级次数
    int32_t updateTimes = 0;

    // 销毁类型
    int32_t destoryType = 0;

    // 属性类型（平行数组）
    std::vector<int32_t> attributeType;

    // 属性值（平行数组）
    std::vector<int32_t> attributeValue;

    // 宝石类型
    std::vector<int32_t> gemType;

    // 宝石等级上限
    std::vector<int32_t> gemLevLimit;

    // 宝石开孔等级
    std::vector<int32_t> gemOpenLev;

    // 强化突破等级
    std::vector<int32_t> intensifyBreakLev;

    // Spine id（可为数字或数组）
    std::vector<int32_t> spineId;

    // 重置随机属性
    std::vector<int32_t> resetRandAttribute;

    // 重置随机属性数量
    std::vector<int32_t> resetRandAttributeNum;

    // 精炼值上限
    std::vector<int32_t> refinedValueMax;

    // 销毁 id（可为数字或数组）
    std::vector<int32_t> destoryId;

    // 销毁数量（可为数字或数组）
    std::vector<int32_t> destoryNum;

    // 精炼道具 id（可为数字或数组）
    std::vector<int32_t> refinedItemId;

    // 精炼道具数量（可为数字或数组）
    std::vector<int32_t> refinedItemNum;

    MG_DEFINE_SERIALIZABLE(id,
                           occupation,
                           position,
                           goodsId,
                           icon,
                           nameId,
                           descId,
                           level,
                           maxLevel,
                           quality,
                           suitId,
                           buffId,
                           bourseId,
                           chipId,
                           forgeId,
                           enchantType,
                           timeType,
                           lastTime,
                           sortWeight,
                           specialAbilityId,
                           randAttributeNum,
                           randAttributeRefinedId,
                           flickerRate,
                           updataItemId,
                           updataItemNum,
                           updateTimes,
                           destoryType,
                           attributeType,
                           attributeValue,
                           gemType,
                           gemLevLimit,
                           gemOpenLev,
                           intensifyBreakLev,
                           spineId,
                           resetRandAttribute,
                           resetRandAttributeNum,
                           refinedValueMax,
                           destoryId,
                           destoryNum,
                           refinedItemId,
                           refinedItemNum);
};

// 时装
class FashionConfig : public Object
{
public:
    typedef Object Super;

public:
    FashionConfig() {}

    virtual ~FashionConfig() {}

    int32_t id = 0;

    // 职业
    int32_t occupation = 0;

    // 部位
    int32_t position = 0;

    // 关联物品 id
    int32_t goodsId = 0;

    // 图标
    std::string icon;

    // 小图标
    std::string miniIcon;

    // 名称文本 id
    int32_t nameId = 0;

    // 描述文本 id
    int32_t descId = 0;

    // 品质
    int32_t quality = 0;

    // 套装 id
    int32_t suitId = 0;

    // UI Spine id
    int32_t spineUiId = 0;

    // UI 皮肤 Spine id
    int32_t spineSkinUiId = 0;

    // 交易所 id
    int32_t bourseId = 0;

    // buff
    int32_t buff = 0;

    // 是否收藏
    int32_t collect = 0;

    // 时装模板
    int32_t fashionTemplate = 0;

    // 评分
    int32_t score = 0;

    // 盾
    int32_t shield = 0;

    // 角色等级
    int32_t roleLevel = 0;

    // 特殊能力
    int32_t specialability = 0;

    // 时间类型
    int32_t timeType = 0;

    // 持续时间
    int32_t lastTime = 0;

    // 销毁类型
    int32_t destoryType = 0;

    // 获取方式类型
    int32_t accessTypeId = 0;

    // 激活属性类型
    std::vector<int32_t> activeAttrType;

    // 激活属性值
    std::vector<int32_t> activeAttrValue;

    // 激活属性值类型
    std::vector<int32_t> activeAttrValueType;

    // 属性类型（平行数组）
    std::vector<int32_t> attributeType;

    // 属性值（平行数组）
    std::vector<int32_t> attributeValue;

    // 大类型
    std::vector<int32_t> bigType;

    // 展示物品
    std::vector<int32_t> showItem;

    // 销毁 id（可为数字或数组）
    std::vector<int32_t> destoryId;

    // 销毁数量（可为数字或数组）
    std::vector<int32_t> destoryNum;

    MG_DEFINE_SERIALIZABLE(id,
                           occupation,
                           position,
                           goodsId,
                           icon,
                           miniIcon,
                           nameId,
                           descId,
                           quality,
                           suitId,
                           spineUiId,
                           spineSkinUiId,
                           bourseId,
                           buff,
                           collect,
                           fashionTemplate,
                           score,
                           shield,
                           roleLevel,
                           specialability,
                           timeType,
                           lastTime,
                           destoryType,
                           accessTypeId,
                           activeAttrType,
                           activeAttrValue,
                           activeAttrValueType,
                           attributeType,
                           attributeValue,
                           bigType,
                           showItem,
                           destoryId,
                           destoryNum);
};

// 时装套装
class FashionSuitConfig : public Object
{
public:
    typedef Object Super;

public:
    FashionSuitConfig() {}

    virtual ~FashionSuitConfig() {}

    int32_t id = 0;

    // 名称文本 id
    int32_t nameId = 0;

    // 职业
    int32_t occupation = 0;

    // 套装品质
    int32_t suitQuality = 0;

    // 盾
    int32_t shield = 0;

    // 列表图
    std::string listPic;

    // 套装部件（时装 id 列表）
    std::vector<int32_t> suitParts;

    MG_DEFINE_SERIALIZABLE(id, nameId, occupation, suitQuality, shield, listPic, suitParts);
};

// 物品区间基表
class ItemBaseConfig : public Object
{
public:
    typedef Object Super;

public:
    ItemBaseConfig() {}

    virtual ~ItemBaseConfig() {}

    int32_t id = 0;

    // 区间下限
    int32_t minId = 0;

    // 区间上限
    int32_t maxId = 0;

    // 物品类型
    int32_t itemType = 0;

    // 名称类型
    int32_t nameType = 0;

    // 英文名称类型
    int32_t nameEnglishType = 0;

    // 堆叠类型
    int32_t stackType = 0;

    // 表名
    std::string tableName;

    MG_DEFINE_SERIALIZABLE(id, minId, maxId, itemType, nameType, nameEnglishType, stackType, tableName);
};

// 时装 Spine 资源
class ResFashionConfig : public Object
{
public:
    typedef Object Super;

public:
    ResFashionConfig() {}

    virtual ~ResFashionConfig() {}

    int32_t id = 0;

    // 皮肤名（源表可为数字或字符串）
    std::string skinName;

    // Spine 路径（空表示无资源）
    std::string spine;

    MG_DEFINE_SERIALIZABLE(id, skinName, spine);
};

NS_MG_END
