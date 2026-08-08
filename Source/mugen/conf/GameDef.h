#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

static const uint32_t INPUT_SLOT_NONE       = 0;   // 无输入槽位
static const uint32_t INPUT_SLOT_MOVE_LEFT  = 1;   // 左移按键槽位
static const uint32_t INPUT_SLOT_MOVE_RIGHT = 2;   // 右移按键槽位
static const uint32_t INPUT_SLOT_MOVE_UP    = 3;   // 上移按键槽位
static const uint32_t INPUT_SLOT_MOVE_DOWN  = 4;   // 下移按键槽位
static const uint32_t INPUT_SLOT_Z          = 5;   // 按键 Z 槽位
static const uint32_t INPUT_SLOT_X          = 6;   // 按键 X 槽位
static const uint32_t INPUT_SLOT_C          = 7;   // 按键 C 槽位
static const uint32_t INPUT_SLOT_0          = 8;   // 技能槽位 0
static const uint32_t INPUT_SLOT_1          = 9;   // 技能槽位 1
static const uint32_t INPUT_SLOT_2          = 10;  // 技能槽位 2
static const uint32_t INPUT_SLOT_3          = 11;  // 技能槽位 3
static const uint32_t INPUT_SLOT_4          = 12;  // 技能槽位 4
static const uint32_t INPUT_SLOT_5          = 13;  // 技能槽位 5
static const uint32_t INPUT_SLOT_6          = 14;  // 技能槽位 6
static const uint32_t INPUT_SLOT_7          = 15;  // 技能槽位 7
static const uint32_t INPUT_SLOT_8          = 16;  // 技能槽位 8
static const uint32_t INPUT_SLOT_9          = 17;  // 技能槽位 9
static const uint32_t INPUT_SLOT_10         = 18;  // 技能槽位 10
static const uint32_t INPUT_SLOT_MAX        = INPUT_SLOT_10 + 1;

// 技能槽位触发模式标志位（可组合），决定哪些输入事件可以激活技能
enum SlotTriggerFlag : uint32_t
{
    kSlotTriggerNone      = 0,       // 不可被槽位触发
    kSlotTriggerPress     = 1 << 0,  // 按键刚按下（上升沿）时触发
    kSlotTriggerKeepPress = 1 << 1,  // 按键持续按住期间每帧触发
    kSlotTriggerRelease   = 1 << 2,  // 按键抬起时触发
};

enum EntityCategory : int8_t
{
    kPlayer,       // 玩家
    kMonster,      // 怪物/NPC
    kSkillEffect,  // 技能创建的效果（投射物、AOE等）
};

// 可创建职业与装备 occupation
enum JobType : int8_t
{
    kUnknown  = 0,
    kSwordman = 1,  // 剑士
    kRanger   = 2,  // 游侠
    kMage     = 4,  // 法师
};

// 角色受击反馈类型
enum HitType : int8_t
{
    kHitNone,    // 无
    kHitLight,   // 普通攻击，小硬直
    kHitHeavy,   // 重击/击晕，大硬直
    kHitLaunch,  // 击飞升空，经过空中阶段再落地翻滚
    kHitDown,    // 直接倒地，跳过空中阶段
    kHitGrab,    // 抓取控制
};

// 角色朝向
enum FacingDirection : int8_t
{
    kFacingLeft,
    kFacingRight,
};

// 最短保底硬直（ms），无视防守方属性也不得低于此值
static constexpr int32_t HITSTUN_MIN_MS = 100;

// 状态标签位掩码
enum StateTag : uint32_t
{
    kTagNone          = 0,
    kTagGrounded      = 1 << 0,  // 地面状态
    kTagMovable       = 1 << 1,  // 可移动
    kTagAirborne      = 1 << 2,  // 空中状态
    kTagHitState      = 1 << 3,  // 受击状态
    kTagDownState     = 1 << 4,  // 倒地状态
    kTagAttackAllowed = 1 << 5,  // 可释放攻击技能
    kTagFacingAllowed = 1 << 6,  // 可转向
    kTagDashState     = 1 << 7,  // 冲刺状态
    kTagFalling       = 1 << 8,  // 下落状态
};

// 行为分支类型
enum class BehaviorKind : int32_t
{
    kIdle     = 0,
    kWalk     = 1,
    kDash     = 2,
    kAttack   = 3,
    kStun     = 4,
    kHitUp    = 5,
    kHitDown  = 6,
    kHitFloor = 7,
    kGetUp    = 8,
    kDeath    = 9,
    kChase    = 10,
    kPatrol   = 11,
    kAlert    = 12,
};

NS_MG_END
