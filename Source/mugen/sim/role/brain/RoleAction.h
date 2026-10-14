#pragma once

#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim
{
class World;
}

namespace mugen::sim::behavior
{
// 非攻击叶子使用的动作类型。
enum class ActionType : uint8_t
{
    // 无动作占位。
    None,
    // 死亡动作。
    Death,
    // 起身动作。
    GetUp,
    // 落地动作。
    HitFloor,
    // 倒地受击动作。
    HitDown,
    // 击飞受击动作。
    HitUp,
    // 受击切换动作。
    HitSwitch,
    // 普通受击动作。
    Hit,
    // 下落动作。
    Fall,
    // 巡逻动作。
    Patrol,
    // 追击动作。
    Chase,
    // 警戒动作。
    Alert,
    // 跑动动作。
    Run,
    // 行走动作。
    Walk,
    // 待机动作。
    Idle,
    // 冲刺动作。
    Spurt,
    // 拾取动作。
    Collect,
    // 激活动作。
    Active,
    // 传送动作。
    Transfer,
    // 通用移动动作。
    Move,
    // 准备待机动作。
    ReadyIdle,
    // 准备移动动作。
    ReadyMove,
    // 被挤开动作。
    Jostled,
    // 跟随动作。
    Follow,
    // 特殊待机动作。
    SpecIdle,
    // 受控动作。
    Controlled,
    // 醒来动作。
    Wake,
    // 复活动作。
    Revive,
    // 销毁动作。
    Destroy
};
// 普通动作叶子的运行时计时。
struct ActionState
{
    // 叶子已运行的逻辑时间。
    Fixed fTime;
    // 上一帧动作时间，用于检测动画循环。
    Fixed lastMotionTime;
    MG_REFLECT(ActionState, fTime, lastMotionTime)
};
// 推挤期间保留入场位移，避免后续状态参数修改影响收尾。
struct JostledState
{
    // 入场时的位移配置。
    int32_t m_displacementId = -1;
    // 制动事件只能完成一次。
    bool m_complete = false;
    MG_REFLECT(JostledState, m_displacementId, m_complete)
};
// 返回动作类型对应的动画名称。
const char* animationName(ActionType type);
// 通知普通动作叶子开始。
void behaviorStateStart(World& world, Entity entity, ActionType type);
// 通知普通动作叶子结束。
void behaviorStateEnd(World& world, Entity entity, ActionType type);
}  // namespace mugen::sim::behavior
