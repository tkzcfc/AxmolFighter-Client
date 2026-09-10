#pragma once

#include "mugen/conf/GameDef.h"
#include "mugen/core/math/Vec2.h"
#include "mugen/core/math/Vec3.h"

// 本文件只放「有 AxmolFighter-Config/table 对应行」的配置结构，以及表行内嵌套辅助类型。
// 运行时状态（如 AttributeComponent / CombatStats）放 component/，勿再塞进此处。
// kind → 黑月源表对照见 AxmolFighter-Tools/config_converter/docs/table-kind-mapping.md

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

    // skill_p 技能点
    int32_t skillP = 0;

    // show_hidden 是否显示隐藏
    int32_t showHidden = 0;

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
                           showHidden,
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

    // 场景 mapKey（转换器从 map_data_id → map_data.map_key 烘培）
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

// 动作攻击段（action_attack；特效动作 action_attack_effect* 共用，多召唤/双音效/数组镜头）
class ActionAttackConfig : public Object
{
public:
    typedef Object Super;

public:
    ActionAttackConfig() {}

    virtual ~ActionAttackConfig() {}

    int32_t id = 0;
    // action spine 动画下标
    int32_t action = 2;
    // aciton_scale_time（表字段少一个 t）播放速率
    float actionScaleTime = 1.0f;
    // action_delay_time 延迟开播
    int32_t actionDelayTime = 0;
    // loop <=1 播一次；>1 或 -1 循环
    int32_t loop = 1;
    // control 1 本段跟摇杆转向
    int32_t control = -1;
    // control_velocity 可控移动速度
    float controlVelocity = 0.0f;
    // custom_vec 自定义方向
    Vector2f customVec;
    // extend_role_vec 1 时特效继承 custom_vec 朝向
    int32_t extendRoleVec = -1;
    // displacement_id 自身位移（特效表则是飞出）
    int32_t displacementId = -1;
    // effect_ids 要刷的 entity_effect id，与 effect_frames 等长
    std::vector<int32_t> effectIds;
    // effect_frames 第几动作帧刷特效
    std::vector<int32_t> effectFrames;
    // buff_ids 本段开始时给自己加的 buff，[1]==-1 不加
    std::vector<int32_t> buffIds;
    // camera_id / camera_frame 震屏（角色表是标量；特效表是数组，同时写入 cameraIds/cameraFrames）
    int32_t cameraId    = -1;
    int32_t cameraFrame = -1;
    std::vector<int32_t> cameraIds;
    std::vector<int32_t> cameraFrames;
    // interrupt_frame 普通打断开放帧
    int32_t interruptFrame = -1;
    // interrupt_extra_frame 至尊打断开放帧（特效表仅有 interrupt_frame）
    int32_t interruptExtraFrame = -1;
    // sound_id 音效
    std::vector<int32_t> soundId;
    // sound_id2 特效动作第二音效
    std::vector<int32_t> soundId2;
    // shadow 影子缩放
    float shadow = 2.0f;
    // floor 贴地
    int32_t floor = -1;
    // obstruct 遇障处理
    int32_t obstruct = -1;
    // ghost -1 无残影
    int32_t ghost = -1;
    // action_orientation 强制朝向
    int32_t actionOrientation = -1;
    // action_pos_type / relative_action_pos 相对落点
    int32_t actionPosType = -1;
    Vector3f relativeActionPos;
    // display_spine_ids / _frame / _frame_count 额外展示 spine
    std::vector<int32_t> displaySpineIds;
    int32_t displaySpineFrame      = 0;
    int32_t displaySpineFrameCount = 0;
    // dialog_show / name_show / tips_show 气泡 / 名字 / 横条
    int32_t dialogShow = -1;
    int32_t nameShow   = 0;
    int32_t tipsShow   = -1;
    // static_target / static_time / static_start_frame / static_reset_time 定身镜头
    int32_t staticTarget     = -1;
    int32_t staticTime       = 0;
    int32_t staticStartFrame = 0;
    int32_t staticResetTime  = 0;
    // transform_id / transform_frame / transform_type 变身
    int32_t transformId    = -1;
    int32_t transformFrame = -1;
    int32_t transformType  = -1;
    // summon_id / summon_frame / summon_time 特效动作：到帧召唤角色，存活时间
    int32_t summonId    = -1;
    int32_t summonFrame = -1;
    int32_t summonTime  = 0;

    MG_DEFINE_SERIALIZABLE(id,
                           action,
                           actionScaleTime,
                           actionDelayTime,
                           loop,
                           control,
                           controlVelocity,
                           customVec,
                           extendRoleVec,
                           displacementId,
                           effectIds,
                           effectFrames,
                           buffIds,
                           cameraId,
                           cameraFrame,
                           cameraIds,
                           cameraFrames,
                           interruptFrame,
                           interruptExtraFrame,
                           soundId,
                           soundId2,
                           shadow,
                           floor,
                           obstruct,
                           ghost,
                           actionOrientation,
                           actionPosType,
                           relativeActionPos,
                           displaySpineIds,
                           displaySpineFrame,
                           displaySpineFrameCount,
                           dialogShow,
                           nameShow,
                           tipsShow,
                           staticTarget,
                           staticTime,
                           staticStartFrame,
                           staticResetTime,
                           transformId,
                           transformFrame,
                           transformType,
                           summonId,
                           summonFrame,
                           summonTime);
};

// 技能定义 skill_attack（施法核心：CD、消耗、动作行、连携、打断优先级）
class SkillAttackConfig : public Object
{
public:
    typedef Object Super;

public:
    SkillAttackConfig() {}

    virtual ~SkillAttackConfig() {}

    int32_t id = 0;
    // action_ids 方向 × 动作序列。外层摇杆象限，内层 action_attack id 列表
    std::vector<IntListRow> actionIds;
    // cd 冷却毫秒
    int32_t cd = 0;
    // pvp_cd PVP 冷却（公平 CD 开关打开时用）
    int32_t pvpCd = 0;
    // cd_count -1 不启用多段充能；>0 为管道数 / 可释放次数
    int32_t cdCount = -1;
    // mp 蓝耗
    int32_t mp = 0;
    // ep EP/TP 耗
    int32_t ep = 0;
    // crystal 晶体耗
    int32_t crystal = 0;
    // sorder 打断优先级。-1 可打断任意技能；表默认 80
    int32_t sorder = 80;
    // sorder_control_type 0 普通打断；1 允许被低优先在 interrupt_frame 后打断；2 无视优先、看 interrupt_extra_frame
    std::vector<int32_t> sorderControlType;
    // skill_ai_id 关联 skill_ai（部分调用仍用 entity_ai.skill_ai_ids 覆盖）
    int32_t skillAiId = 201;
    // next_skill 同槽下一步技能 id，-1 无
    int32_t nextSkill = -1;
    // name_id / desc_id 文本表
    int32_t nameId = 1;
    int32_t descId = 1;
    // icon 战斗 HUD 图标（表里可能是资源 id 或路径；路径在转换时变成 0）
    int32_t icon = -1;
    // type -1 普通（受击中只能放挣脱）；0 反击；1 表注释写暂时无效
    int32_t type = -1;
    // press_time 蓄力门槛毫秒。抬起时若技能时间未到则走 up_action_ids
    int32_t pressTime = 0;
    // rage 怒气伙伴命中加怒，0 不加
    int32_t rage = 0;
    // skill_type 0 按下释放；1 抬起释放
    int32_t skillType = 0;
    // up_action_ids 抬起释放方向 × 动作序列，形状同 actionIds
    std::vector<IntListRow> upActionIds;

    MG_DEFINE_SERIALIZABLE(id,
                           actionIds,
                           cd,
                           pvpCd,
                           cdCount,
                           mp,
                           ep,
                           crystal,
                           sorder,
                           sorderControlType,
                           skillAiId,
                           nextSkill,
                           nameId,
                           descId,
                           icon,
                           type,
                           pressTime,
                           rage,
                           skillType,
                           upActionIds);
};

// 命中结果 skill_hit（伤害系数 + 受击反应）
class SkillHitTableConfig : public Object
{
public:
    typedef Object Super;

public:
    SkillHitTableConfig() {}

    virtual ~SkillHitTableConfig() {}

    int32_t id = 0;
    // hurt_type 0 物理 1 魔法 2 自适应 3 真伤
    int32_t hurtType = 0;
    // hurt_rate PVE 伤害系数，乘在标准伤害上
    float hurtRate = 1.0f;
    // pvp_hurt_rate PVP 系数
    float pvpHurtRate = 1.0f;
    // hit_type -1 碰到但不掉血不硬直；0 击退；1 击倒；2 击飞
    int32_t hitType = 2;
    // hit_condition -1 任意状态；0 不打倒地；1 不打浮空；2 倒地和浮空都不打
    int32_t hitCondition = -1;
    // hit_must 0 可被属性闪避；1 必中
    int32_t hitMust = 0;
    // hit_rigidity 破霸体/削韧相关计数
    int32_t hitRigidity = 0;
    // hit_counts 该特效最多命中次数；-1 不限
    int32_t hitCounts = -1;
    // hit_interval -1 同一目标只中一次；>0 为对同一目标再命中的毫秒间隔
    int32_t hitInterval = -1;
    // stiff_time 硬直毫秒
    int32_t stiffTime = 800;
    // freeze_time 命中冻结（逻辑停）
    int32_t freezeTime = 1;
    // freeze_time_delay 延迟再冻
    int32_t freezeTimeDelay = 0;
    // freeze_time_control_role 0 连攻击者也冻
    int32_t freezeTimeControlRole = 0;
    // freeze_time_control_effect 0 连特效也冻
    int32_t freezeTimeControlEffect = 0;
    // displacement_id 地面击退 → action_displacement
    int32_t displacementId = -1;
    // air_displacement_id 浮空时
    int32_t airDisplacementId = -1;
    // floor_displacement_id 倒地时
    int32_t floorDisplacementId = -1;

    MG_DEFINE_SERIALIZABLE(id,
                           hurtType,
                           hurtRate,
                           pvpHurtRate,
                           hitType,
                           hitCondition,
                           hitMust,
                           hitRigidity,
                           hitCounts,
                           hitInterval,
                           stiffTime,
                           freezeTime,
                           freezeTimeDelay,
                           freezeTimeControlRole,
                           freezeTimeControlEffect,
                           displacementId,
                           airDisplacementId,
                           floorDisplacementId);
};

// 属性曲线 entity_attribute
class AttributeTemplateConfig : public Object
{
public:
    typedef Object Super;

public:
    AttributeTemplateConfig() {}

    virtual ~AttributeTemplateConfig() {}

    int32_t id = 0;
    // source_force / agility / habitus / spirit 四维主属性
    float sourceForce = 0;
    float agility     = 0;
    float habitus     = 0;
    float spirit      = 0;
    // hp / atk / def / matk / mdef 战斗属性
    float hp   = 0;
    float atk  = 0;
    float def  = 0;
    float matk = 0;
    float mdef = 0;
    // crit / crit_resist / crit_damage / crit_damage_resist
    float crit             = 0;
    float critResist       = 0;
    float critDamage       = 0;
    float critDamageResist = 0;
    // dodge / hit 闪避/命中
    float dodge = 0;
    float hit   = 0;
    // base_damage 怪物标准伤害底
    float baseDamage = 0;
    // monster_hit_number / player_hit_number 期望受击次数，拉 HP/伤害
    float monsterHitNumber = 0;
    float playerHitNumber  = 0;
    // elite_hp_rate / elite_hurt_rate 精英
    float eliteHpRate   = 1.6f;
    float eliteHurtRate = 1.2f;
    // boss_hp_rate / boss_hurt_rate Boss
    float bossHpRate   = 0;
    float bossHurtRate = 1.6f;

    MG_DEFINE_SERIALIZABLE(id,
                           sourceForce,
                           agility,
                           habitus,
                           spirit,
                           hp,
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
                           baseDamage,
                           monsterHitNumber,
                           playerHitNumber,
                           eliteHpRate,
                           eliteHurtRate,
                           bossHpRate,
                           bossHurtRate);
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

    MG_DEFINE_SERIALIZABLE(id, spine, scale);
};

// 位移曲线 action_displacement（速度/加速度都是三维向量，时间毫秒）
class DisplacementConfig : public Object
{
public:
    typedef Object Super;

    DisplacementConfig() {}
    virtual ~DisplacementConfig() {}

    int32_t id = 0;
    // velocity 初速 [x,y,z]
    Vector3f velocity;
    // velocity_time 该速度持续
    Vector3i velocityTime;
    // acceleration 加速度
    Vector3f acceleration;
    // acceleration_time 加速持续
    Vector3i accelerationTime;
    // gravity 重力开关/系数
    float gravity = 1.0f;
    // bounces 落地弹跳衰减
    float bounces = 0.5f;
    // is_trace >0 追踪
    int32_t isTrace = 0;
    // trace_angle / trace_radius / trace_velocity 扇形寻敌再改速度
    float traceAngle    = 0;
    float traceRadius   = 0;
    float traceVelocity = 0;

    MG_DEFINE_SERIALIZABLE(id,
                           velocity,
                           velocityTime,
                           acceleration,
                           accelerationTime,
                           gravity,
                           bounces,
                           isTrace,
                           traceAngle,
                           traceRadius,
                           traceVelocity);
};

// 震屏 action_camera
class CameraConfig : public Object
{
public:
    typedef Object Super;

    CameraConfig() {}
    virtual ~CameraConfig() {}

    int32_t id = 0;
    // amplitude_x / amplitude_y 振幅
    float amplitudeX = 0;
    float amplitudeY = 0;
    // duration 时长毫秒
    float duration = 100;
    // times 震荡次数
    int32_t times = 10;
    // level 优先级，高的盖低的
    int32_t level = 10;
    // modifier 衰减曲线标记
    std::string modifier = "i";

    MG_DEFINE_SERIALIZABLE(id, amplitudeX, amplitudeY, duration, times, level, modifier);
};

// 技能特效 entity_effect / entity_effect2（id≥2000000 走 effect2）
class EffectConfig : public Object
{
public:
    typedef Object Super;

    EffectConfig() {}
    virtual ~EffectConfig() {}

    int32_t id = 0;
    // action_ids → action_attack_effect*，EFFECT 树按序播
    std::vector<int32_t> actionIds;
    // hit_id → skill_hit；-1 不造成伤害
    int32_t hitId = -1;
    // hit_effect_ids 打中后的受击特效
    std::vector<int32_t> hitEffectIds;
    // hit_target -1 双方 0 敌 1 友
    int32_t hitTarget = -1;
    // hit_extra_control 能否打无敌/起身
    std::vector<int32_t> hitExtraControl;
    // res_spine_id 外观
    int32_t resSpineId = 1702;
    // relative_position / spine_relative_position 相对施法者
    Vector3f relativePosition;
    Vector3f spineRelativePosition;
    // radius 碰撞半径
    float radius = 20.0f;
    // velocity 初速（可被位移表覆盖）
    float velocity = 0.0f;
    // collision 是否注册碰撞
    int32_t collision = 1;
    // follow 0 不跟随；1/2 跟随且可随技能打断回收
    int32_t follow = 0;
    // control 是否可被操作
    int32_t control = 0;
    // auto_release 0 动作结束就销毁；1 常与 follow 一起在技能打断时回收；2/3 命中次数到了销毁
    int32_t autoRelease = 1;
    // effect_type / position_type 生成位置规则
    int32_t effectType   = -1;
    int32_t positionType = -1;
    // effect_oriebtation_X/Z 朝向（表字段拼写 oriebtation）
    int32_t effectOriebtationX = -1;
    int32_t effectOriebtationZ = -1;
    // buff_id / debuff_id / buff_all_id 命中友/敌/全体 Buff
    std::vector<int32_t> buffId;
    std::vector<int32_t> debuffId;
    int32_t buffAllId = -1;
    // specialability_id / specialability_all_id 命中触发 SA
    int32_t specialabilityId    = -1;
    int32_t specialabilityAllId = -1;
    // next_effect_id 销毁后再刷一个
    int32_t nextEffectId = -1;
    // combo_exp 连击经验
    int32_t comboExp = 10;
    // energy 命中回 EP
    int32_t energy = 0;
    // shadow / tier
    int32_t shadow = 0;
    int32_t tier   = 2;
    // preload_count 预加载池大小
    int32_t preloadCount = 1;

    MG_DEFINE_SERIALIZABLE(id,
                           actionIds,
                           hitId,
                           hitEffectIds,
                           hitTarget,
                           hitExtraControl,
                           resSpineId,
                           relativePosition,
                           spineRelativePosition,
                           radius,
                           velocity,
                           collision,
                           follow,
                           control,
                           autoRelease,
                           effectType,
                           positionType,
                           effectOriebtationX,
                           effectOriebtationZ,
                           buffId,
                           debuffId,
                           buffAllId,
                           specialabilityId,
                           specialabilityAllId,
                           nextEffectId,
                           comboExp,
                           energy,
                           shadow,
                           tier,
                           preloadCount);
};

// Buff 规则 buff_rule：rule_id → class_name
class BuffRuleConfig : public Object
{
public:
    typedef Object Super;

    BuffRuleConfig() {}
    virtual ~BuffRuleConfig() {}

    int32_t id = 0;
    // buff_type 1 在表里标 DoT 类状态
    int32_t buffType = 0;
    // class_name BuffPool:addBuff new(class_name)；默认 BuffAddByApplicator
    std::string className;
    // fashion_show_text
    int32_t fashionShowText = 1;

    MG_DEFINE_SERIALIZABLE(id, buffType, className, fashionShowText);
};

// Buff 实例 buff_base
class BuffConfig : public Object
{
public:
    typedef Object Super;

    BuffConfig() {}
    virtual ~BuffConfig() {}

    int32_t id = 0;
    // rule_id → buff_rule，决定逻辑类
    int32_t ruleId = 100;
    // buff_type 大类（0 通常增益/功能，DoT 行常为 1）
    int32_t buffType = 0;
    // sub_type 与 rule_id 一起当叠加 key；-1 每次都新建
    int32_t subType = -1;
    // add_type 叠加策略（0 挂起 1 续 2 删）
    int32_t addType = 0;
    // target 1 自己 2 敌 3 友 4 队员 5 敌营 6 Boss 7 精英 8 事件目标自己 9 全营 10 敌营除自己
    int32_t target = 1;
    // began / ended 监听的 BFEvent id，-1 不靠事件
    int32_t began = -1;
    int32_t ended = -1;
    // event_param 事件过滤参数
    std::vector<int32_t> eventParam;
    // condition / condition_param EnumBuffCondition + 参数
    std::vector<int32_t> condition;
    std::vector<IntListRow> conditionParam;
    // execute_type 1 加载即 ENTER；2 激活等间隔/事件；3 等事件才激活
    int32_t executeType = 1;
    // param_value / param_value2 规则专用数值
    std::vector<float> paramValue;
    std::vector<float> paramValue2;
    // interval 周期毫秒
    int32_t interval = 0;
    // times 可触发次数；-1 常见于无限
    int32_t times = 0;
    // repeat_max 层数上限
    int32_t repeatMax = 1;
    // remove_repeat_all 移除时是否清全部层
    int32_t removeRepeatAll = 1;
    // probability 添加成功率
    int32_t probability = 100;
    // probability_repeat 1 时成功率随层数乘
    int32_t probabilityRepeat = 0;
    // priority 同 rule+sub 替换门槛
    int32_t priority = 1;
    // cd / cd_pvp 添加后的 Buff CD
    int32_t cd    = 0;
    int32_t cdPvp = 0;
    // inner_cd 内部触发 CD
    int32_t innerCd = 0;
    // binding 1 绑定技能，技能卸下带走
    int32_t binding = 0;
    // bind_special_ability_id 挂上时绑定 SA
    int32_t bindSpecialAbilityId = -1;
    // inherit 召唤物/变身继承
    int32_t inherit = 0;
    // reset_type / destroy_type 重置与销毁时机
    int32_t resetType   = 0;
    int32_t destroyType = 0;
    // area_setting 允许生效的战斗区域
    std::vector<int32_t> areaSetting;
    // buff_direction 朝向相关
    int32_t buffDirection = 0;
    // buff_partner 是否给伙伴
    int32_t buffPartner = 0;
    // hurt_type Buff 伤类型（部分 DoT）
    int32_t hurtType = 2;
    // spine_id / spine_offsets / spine_step 身上特效
    int32_t spineId = -1;
    std::vector<float> spineOffsets;
    std::vector<int32_t> spineStep;
    // spine_layer 0 人物前；1 人物后
    int32_t spineLayer = 0;
    // icon / icon_desc_id / name_id / desc_id UI
    int32_t icon       = -1;
    int32_t iconDescId = 0;
    int32_t nameId     = 1;
    int32_t descId     = 1;
    // show_tips 1 出图标提示
    int32_t showTips = 0;
    // audio_id 音效
    int32_t audioId = -1;

    MG_DEFINE_SERIALIZABLE(id,
                           ruleId,
                           buffType,
                           subType,
                           addType,
                           target,
                           began,
                           ended,
                           eventParam,
                           condition,
                           conditionParam,
                           executeType,
                           paramValue,
                           paramValue2,
                           interval,
                           times,
                           repeatMax,
                           removeRepeatAll,
                           probability,
                           probabilityRepeat,
                           priority,
                           cd,
                           cdPvp,
                           innerCd,
                           binding,
                           bindSpecialAbilityId,
                           inherit,
                           resetType,
                           destroyType,
                           areaSetting,
                           buffDirection,
                           buffPartner,
                           hurtType,
                           spineId,
                           spineOffsets,
                           spineStep,
                           spineLayer,
                           icon,
                           iconDescId,
                           nameId,
                           descId,
                           showTips,
                           audioId);
};

// 角色 AI 参数 entity_ai
class AiConfig : public Object
{
public:
    typedef Object Super;

    AiConfig() {}
    virtual ~AiConfig() {}

    int32_t id = 0;
    // target_scope_x/z 索敌范围
    Vector2i targetScopeX;
    Vector2i targetScopeZ;
    // chase_scope_x/z 追击范围
    Vector2i chaseScopeX;
    Vector2i chaseScopeZ;
    // patrol_scope_x/z 巡逻范围
    Vector2i patrolScopeX;
    Vector2i patrolScopeZ;
    // alert_delay_time / chase_delay_time / patrol_delay_time 状态切换延迟
    Vector2i alertDelayTime;
    Vector2i chaseDelayTime;
    Vector2i patrolDelayTime;
    // skill_ids Normal 组（转换器按槽取每组第一个 skill_id，供当前 AI 使用）
    std::vector<int32_t> skillIds;
    // skill_ai_ids 与 skill_ids 对齐的 skill_ai
    std::vector<int32_t> skillAiIds;
    // crazy_skill_ids / crazy_skill_ai_ids 爆气组
    std::vector<int32_t> crazySkillIds;
    std::vector<int32_t> crazySkillAiIds;
    // joystick_skill_ids 搓招组
    std::vector<int32_t> joystickSkillIds;
    // other_skill_ids Special 组
    std::vector<int32_t> otherSkillIds;
    // skill_interval 全局放技能间隔
    int32_t skillInterval = 0;
    // skill_priority_level 每槽优先级
    std::vector<int32_t> skillPriorityLevel;
    // skill_priority_level_cd 该槽 AI 间隔
    std::vector<int32_t> skillPriorityLevelCd;

    MG_DEFINE_SERIALIZABLE(id,
                           targetScopeX,
                           targetScopeZ,
                           chaseScopeX,
                           chaseScopeZ,
                           patrolScopeX,
                           patrolScopeZ,
                           alertDelayTime,
                           chaseDelayTime,
                           patrolDelayTime,
                           skillIds,
                           skillAiIds,
                           crazySkillIds,
                           crazySkillAiIds,
                           joystickSkillIds,
                           otherSkillIds,
                           skillInterval,
                           skillPriorityLevel,
                           skillPriorityLevelCd);
};

// 自动释放条件 skill_ai
class SkillAiConfig : public Object
{
public:
    typedef Object Super;

    SkillAiConfig() {}
    virtual ~SkillAiConfig() {}

    int32_t id = 0;
    // load_cd 进场后多久才允许第一次判定（毫秒）
    int32_t loadCd = 0;
    // check_cd 两次判定间隔
    int32_t checkCd = 0;
    // prob 通过其它条件后的成功率 0–100
    int32_t prob = 100;
    // use_count -1 无限；>0 用一次减一
    int32_t useCount = -1;
    // composition [1] AND 组，[2] OR 组。元素是检查函数下标
    std::vector<IntListRow> composition;
    // opp_dis_x / opp_dis_z 与目标轴距区间
    Vector2i oppDisX;
    Vector2i oppDisZ;
    // opp_status 目标状态，-1 任意
    int32_t oppStatus = 6;
    // self_status 自身状态
    int32_t selfStatus = -1;
    // opp_combo -1 不检查；否则要求目标正在连携
    int32_t oppCombo = 0;
    // opp_skill_id 目标当前技能 id
    int32_t oppSkillId = -1;
    // self_hp 自身 HP 百分比
    Vector2i selfHp;

    MG_DEFINE_SERIALIZABLE(id,
                           loadCd,
                           checkCd,
                           prob,
                           useCount,
                           composition,
                           oppDisX,
                           oppDisZ,
                           oppStatus,
                           selfStatus,
                           oppCombo,
                           oppSkillId,
                           selfHp);
};

// 等级标准伤害 skill_hurt（按人物等级取平坦伤害加数，不是每条技能独立表）
class SkillHurtConfig : public Object
{
public:
    typedef Object Super;

    SkillHurtConfig() {}
    virtual ~SkillHurtConfig() {}

    // id 等级（或等级带）
    int32_t id = 0;
    // hurt 英雄标准伤害加数（加在 ATK 上）
    int32_t hurt = 800;
    // monster_hurt 怪物标准伤害加数（加在 base_damage 上）
    int32_t monsterHurt = 0;
    // atk_standard / crit_standard / crit_damage_standard / dodge_standard 平衡曲线，现伤害公式未用
    int32_t atkStandard        = 0;
    int32_t critStandard       = 0;
    int32_t critDamageStandard = 0;
    int32_t dodgeStandard      = 0;

    MG_DEFINE_SERIALIZABLE(id, hurt, monsterHurt, atkStandard, critStandard, critDamageStandard, dodgeStandard);
};

// 英雄 / 怪物模板 entity_role
class RoleConfig : public Object
{
public:
    typedef Object Super;

public:
    RoleConfig() {}

    virtual ~RoleConfig() {}

    // id 角色配置 id（英雄常用 0 号模板再被运行时数据覆盖）
    int32_t id = 0;
    // role_type EntityRoleType（英雄/怪/Boss/召唤…）
    EntityRoleType roleType = EntityRoleType::kBoss;
    // role_type_sign 子类型标记
    std::vector<int32_t> roleTypeSign;
    // name_id / desc_id
    int32_t nameId = 117;
    int32_t descId = 1;
    // res_spine_id / res_spine_id_ext 身体 / 额外 spine
    int32_t resSpineId    = 0;
    int32_t resSpineIdExt = -1;
    // res_fashion 时装
    std::vector<int32_t> resFashion;
    // ai_id 按难度选 entity_ai
    std::vector<int32_t> aiIds;
    // attribute_rate 乘在属性上的系数
    std::vector<float> attributeRate;
    // velocity 移速
    float velocity = 0.25f;
    // radius 逻辑半径
    int32_t radius = 30;
    // weight 重量
    float weight = 0.05f;
    // rigidity 韧性；连打计数
    int32_t rigidity = 24;
    // hit_stiff_time 覆盖 skill_hit.stiff_time；非 NIL 则用这个
    int32_t hitStiffTime = -1;
    // hit_displacement_id -1 用 hit 表；0 不击退；>0 用本 id
    int32_t hitDisplacementId = -1;
    // hit_restrain 受击抑制
    std::vector<int32_t> hitRestrain;
    // hit_count 可被连打次数类限制
    int32_t hitCount = -1;
    // death_displacement_id / death_effect_id 死亡
    int32_t deathDisplacementId = 133;
    int32_t deathEffectId       = 8;
    // ko_effect_id KO 特效
    int32_t koEffectId = -1;
    // buff_ids / buff_pos / buff_scale 进场自带 Buff 及图标位置
    std::vector<int32_t> buffIds;
    std::vector<int32_t> buffPos;
    float buffScale = 1.35f;
    // hp_bar_count 血条管数
    int32_t hpBarCount = 1;
    // monster_camps 默认阵营
    int32_t monsterCamps = 8;
    // shadow / tier / tier_ext 影子与层级
    int32_t shadow  = 2;
    int32_t tier    = 2;
    int32_t tierExt = 2;
    // sound_id / sound_type 语音
    std::vector<int32_t> soundId;
    int32_t soundType = 1;
    // head_image 头像
    std::string headImage;
    // dialog_pos / hurt_num_pos / vertex_pos UI 锚点
    Vector2i dialogPos;
    Vector2i hurtNumPos;
    Vector2i vertexPos;
    // relative_position / spine_relative_position
    Vector3f relativePosition;
    Vector3f spineRelativePosition;
    // time_rage 狂暴：时间 + Buff 列表
    std::vector<int32_t> timeRage;
    // fatigue 疲劳
    int32_t fatigue = 200;
    // is_pass_room 是否算过房
    int32_t isPassRoom = -1;

    MG_DEFINE_SERIALIZABLE(id,
                           roleType,
                           roleTypeSign,
                           nameId,
                           descId,
                           resSpineId,
                           resSpineIdExt,
                           resFashion,
                           aiIds,
                           attributeRate,
                           velocity,
                           radius,
                           weight,
                           rigidity,
                           hitStiffTime,
                           hitDisplacementId,
                           hitRestrain,
                           hitCount,
                           deathDisplacementId,
                           deathEffectId,
                           koEffectId,
                           buffIds,
                           buffPos,
                           buffScale,
                           hpBarCount,
                           monsterCamps,
                           shadow,
                           tier,
                           tierExt,
                           soundId,
                           soundType,
                           headImage,
                           dialogPos,
                           hurtNumPos,
                           vertexPos,
                           relativePosition,
                           spineRelativePosition,
                           timeRage,
                           fatigue,
                           isPassRoom);
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

    // 皮肤名
    std::string skinName;

    // Spine 路径（空表示无资源）
    std::string spine;

    MG_DEFINE_SERIALIZABLE(id, skinName, spine);
};

NS_MG_END
