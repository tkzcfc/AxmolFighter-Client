#pragma once

#include <cstdint>

namespace mugen
{
// 角色可叠加的运行状态位。
enum class EntityRoleStatus : uint32_t
{
    None       = 0x00000000,
    Birth      = 0x00000001,
    Death      = 0x00000002,
    Wake       = 0x00000004,
    GetUp      = 0x00000008,
    HitFloor   = 0x00000010,
    HitDown    = 0x00000020,
    HitUp      = 0x00000040,
    HitSwitch  = 0x00000080,
    Hit        = 0x00000100,
    Attack     = 0x00000200,
    Fall       = 0x00000400,
    Patrol     = 0x00000800,
    Chase      = 0x00001000,
    Alert      = 0x00002000,
    Run        = 0x00004000,
    Walk       = 0x00008000,
    Idle       = 0x00010000,
    Finding    = 0x00020000,
    Jostled    = 0x00040000,
    Revive     = 0x00080000,
    Controlled = 0x00100000,
};

// 合并两个角色状态位集合。
constexpr EntityRoleStatus operator|(EntityRoleStatus left, EntityRoleStatus right)
{
    return static_cast<EntityRoleStatus>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

// 角色的额外控制或强化状态。
enum class ExtraState : int32_t
{
    Durance = 1,
    Palsy,
    Stun,
    Silence,
    SuperArmor,
    Invincible,
    Stealth,
    Frozen
};

// 战斗流程中可触发的事件类型。
enum class BFEvent : int32_t
{
    BeforePrepareSkill = 1,
    BeforeCastSkill,
    BeforeHit,
    AfterHit,
    BeforeToBeHit,
    AfterToBeHit,
    AfterCastSkill,
    AfterPrepareSkill,
    BeforeNextSkill,
    AfterNextSkill,
    BeforeCrazy,
    AfterCrazy,
    EvadeSuccess,
    BeforeDeath,
    EpZero,
    UseTp,
    ShieldClean,
    AttackMiss,
    SkillColdStart,
    SkillColdEnd,
    HpChange,
    UseEp,
    BeforeArtifactSkill,
    AfterArtifactSkill,
    AddBuff,
    RemoveBuff,
    BeforeRoleSkillAction,
    AfterRoleSkillAction,
    BehaviorStateStart = 50,
    BehaviorStateEnd,
    Enter = 100,
    Exit,
    OnceUpdate,
};

// 玩家或系统发出的控制事件。
enum class ControlEvent : uint8_t
{
    CTR_NONE = 0,
    CTR_IDLE = 1,
    CTR_WALK,
    CTR_RUN,
    CTR_REVIVE,
    CTR_DEATH,
    CTR_GOD_1,
    CTR_GOD_2,
    CTR_ADD = 50,
    CTR_DELETE,
    CTR_ATKA = 100,
    CTR_ATKB,
    CTR_ATKC,
    CTR_ATKD,
    CTR_ATKE,
    CTR_ATKF,
    CTR_ATKG,
    CTR_ATKH,
    CTR_ATKI,
    CTR_ATKJ,
    CTR_ATKK,
    CTR_ATKL,
    CTR_ATKM,
    CTR_ATKN,
    CTR_ATKO,
    CTR_ATKQ,
    CTR_JOY1 = 150,
    CTR_JOY2,
    CTR_JOY3,
    CTR_JOY4,
    CTR_JOY5,
    CTR_JOY6,
    CTR_JOY7,
    CTR_JOY8,
    CTR_JOY9,
    CTR_JOY10,
    CTR_JOY11,
    CTR_JOY12,
    CTR_JOYB = 200,
    CTR_JOYC,
    CTR_JOYD,
};

// 技能在角色技能栏中的槽位。
enum class EnumSkillSlot : int32_t
{
    None = 0,
    Attack,
    Sprint,
    Dodge,
    Crazy,
    A,
    B,
    C,
    SA,
    SB,
    SC,
    EA,
    EB,
    SUA,
    SUB,
    SUC,
    Break,
    SAP,
    SBP,
    SCP,
    Anger,
    Max = Anger,
};
// 角色技能的配置分组。
enum class EntitySkillGroup : int32_t
{
    Normal = 1,
    Crazy,
    JoyStick,
    JoyStickCrazy,
    Special
};
// 技能切换模式。
enum class EntitySkillChange : int32_t
{
    Normal = 1,
    Joy,
    Anger
};
// 房间流程状态。
enum class RoomState : int32_t
{
    Check      = -1,
    StoryDoing = -2,
    None       = 0,
    Init,
    Ready,
    Enter,
    ReadySpine,
    Tip,
    Process,
    FightFailure,
    FightRevive,
    FightKo,
    FightVictory,
    FightBalance,
    FightFinish,
    FightFinalBalance,
    FightEnd,
    End,
};
// 房间采用的战斗规则。
enum class EnumBattleRule : int32_t
{
    None = 0,
    Clear,
    Defense,
    Survive,
    Chasm
};
// 配置表使用的角色类型值。
enum class role_type : int32_t
{
    Hero = 1,
    Monster,
    Elite,
    Boss,
    Summon,
    Machine,
    CopperOre,
    SilverOre,
    GoldOre,
    Athena,
    MonsterMachine,
    Crystal,
    Max,
};
// 运行时角色类型值。
enum class EntityRoleType : int32_t
{
    kHero = 1,
    kMonster,
    kElite,
    kBoss,
    kSummon,
    kMachine,
    kCopperOre,
    kSilverOre,
    kGoldOre,
    kAthena,
    kMonsterMachine,
    kCrystal,
    kMax,
};
// 可选角色职业。
enum CharacterClass : int8_t
{
    kUnknown = 0,
    kSwordman,
    kRanger,
    kFighter,
    kMage,
    kCount = 4
};
// 时装可装备的外观部位。
enum class FashionPosition : int32_t
{
    kBody     = 0,
    kHat      = 1,
    kClothes  = 2,
    kWeapon   = 3,
    kHair     = 4,
    kFace     = 5,
    kWing     = 6,
    kSkin     = 7,
    kSkinType = 8,
    kHalo     = 9,
    kPhantom  = 10,
    kCount    = 11,
};
// 角色行为状态机的行为种类。
enum class BehaviorKind : int32_t
{
    kIdle = 0,
    kWalk,
    kDash,
    kAttack,
    kStun,
    kHitUp,
    kHitDown,
    kHitFloor,
    kGetUp,
    kDeath,
    kChase,
    kPatrol,
    kAlert,
    kHitSwitch,
    kJostled,
    kRevive,
    kWake,
    kPathFinding,
};
// 实体所属阵营位。
enum class EntityCamp : uint32_t
{
    Neutral     = 0x0001,
    Our         = 0x0002,
    OurFriend   = 0x0004,
    Enemy       = 0x0008,
    EnemyFriend = 0x0010
};
// 各阵营对应的敌对阵营位集合。
enum class EntityHostileCamp : uint32_t
{
    Neutral     = 0x001e,
    Our         = 0x0019,
    OurFriend   = 0x0019,
    Enemy       = 0x0007,
    EnemyFriend = 0x0007
};
}  // namespace mugen
