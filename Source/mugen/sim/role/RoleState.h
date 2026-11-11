#pragma once
#include "mugen/conf/GameDef.h"

namespace mugen::sim
{
// 角色状态机的单值状态（DESIGN.md §6.4，P-3）。
enum class RoleStateId : uint8_t
{
    Idle = 0,
    Walk,
    Run,
    Patrol,
    Chase,
    Alert,
    Finding,
    Jostled,
    Attack,
    Hit,
    HitSwitch,
    HitUp,
    HitDown,
    HitFloor,
    GetUp,
    Wake,
    Death,
    Revive,
    Puppet
};
namespace role
{
// 由单值状态派生旧实现的位掩码语义（P-3），供条件读取与摘要导出沿用旧状态位名称。
inline EntityRoleStatus statusBits(RoleStateId state)
{
    using S = RoleStateId;
    using B = EntityRoleStatus;
    switch (state)
    {
    case S::Idle:
        return B::Idle;
    case S::Walk:
        return B::Walk | B::Idle;
    case S::Run:
        return B::Run | B::Idle;
    case S::Patrol:
        return B::Patrol | B::Idle;
    case S::Chase:
        return B::Chase | B::Idle;
    case S::Alert:
        return B::Alert | B::Idle;
    case S::Finding:
        return B::Finding | B::Idle;
    case S::Jostled:
        return B::Jostled | B::Idle;
    case S::Attack:
        return B::Attack | B::Idle;
    case S::Hit:
        return B::Hit | B::Idle;
    case S::HitSwitch:
        return B::HitSwitch | B::Idle;
    case S::HitUp:
        return B::HitUp | B::Idle;
    case S::HitDown:
        return B::HitDown | B::Idle;
    case S::HitFloor:
        return B::HitFloor | B::Idle;
    case S::GetUp:
        return B::GetUp | B::Idle;
    case S::Wake:
        return B::Wake | B::Idle;
    case S::Death:
        return B::Death;
    case S::Revive:
        return B::Revive;
    case S::Puppet:
        return B::Controlled;
    }
    return B::Idle;
}
// 由状态位还原单值状态。无法唯一对应时回到 Idle。
inline RoleStateId stateFromStatus(EntityRoleStatus status)
{
    for (uint8_t index = 0; index <= static_cast<uint8_t>(RoleStateId::Puppet); ++index)
    {
        const auto state = static_cast<RoleStateId>(index);
        if (statusBits(state) == status)
            return state;
    }
    return RoleStateId::Idle;
}
// 状态对应的动作名称（与旧实现 ActionNames::animationName 一致）。
inline const char* motionName(RoleStateId state)
{
    switch (state)
    {
    case RoleStateId::Run:
        return "walk";
    case RoleStateId::Walk:
        return "running";
    case RoleStateId::Idle:
    case RoleStateId::Wake:
    case RoleStateId::Revive:
        return "stand";
    case RoleStateId::Patrol:
    case RoleStateId::Chase:
    case RoleStateId::Alert:
    case RoleStateId::Finding:
        return "running";
    case RoleStateId::Death:
        return "death";
    case RoleStateId::GetUp:
        return "getup";
    case RoleStateId::HitFloor:
        return "toground";
    case RoleStateId::HitDown:
        return "flydown";
    case RoleStateId::HitUp:
        return "flyup";
    case RoleStateId::HitSwitch:
        return "hitswitch";
    case RoleStateId::Hit:
        return "hit";
    case RoleStateId::Attack:
    case RoleStateId::Jostled:
    case RoleStateId::Puppet:
    default:
        return "stand";
    }
}
// 是否属于受击族状态。
inline bool isHitState(RoleStateId state)
{
    switch (state)
    {
    case RoleStateId::Hit:
    case RoleStateId::HitSwitch:
    case RoleStateId::HitUp:
    case RoleStateId::HitDown:
    case RoleStateId::HitFloor:
    case RoleStateId::GetUp:
    case RoleStateId::Wake:
        return true;
    default:
        return false;
    }
}
// 是否属于“移动类”状态（与旧实现 RoleStatus::isMove 一致）。
inline bool isMoveState(RoleStateId state)
{
    switch (state)
    {
    case RoleStateId::Patrol:
    case RoleStateId::Chase:
    case RoleStateId::Alert:
    case RoleStateId::Run:
    case RoleStateId::Walk:
    case RoleStateId::Finding:
        return true;
    default:
        return false;
    }
}
}  // namespace role
}  // namespace mugen::sim
