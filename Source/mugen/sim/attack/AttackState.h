#pragma once
#include "mugen/core/ecs/Entity.h"

namespace mugen::sim::attack
{
/// 单个动作攻击叶子的逐帧运行状态。
struct ActionAttack
{
    /// 自动作开始经过的时间。
    Fixed m_time;
    /// 一个动画帧对应的逻辑时间间隔。
    Fixed m_interval;
    /// 动画是否已经达到配置的结束循环次数。
    bool m_animationEnd = false;
    /// 动作是否已经满足行为树结束条件。
    bool m_finished = false;
    /// 按效果配置顺序保存生成的实体。
    std::vector<Entity> m_effects;
    /// 与效果配置平行的生成标记。
    std::vector<uint8_t> m_spawned;
    /// 与镜头配置平行的触发标记。
    std::vector<uint8_t> m_shaken;
    MG_REFLECT(ActionAttack, m_time, m_interval, m_animationEnd, m_finished, m_effects, m_spawned, m_shaken)
};
/// 攻击行为树叶子保存的动作与静止状态。
struct AttackRole
{
    /// 当前动作的逐帧状态。
    ActionAttack m_action;
    /// 是否已显示本动作要求的骨骼部件。
    bool m_displayed = false;
    /// 是否已记录未接入的变身请求。
    bool m_transformLogged = false;
    /// 本次静止效果可再次触发的世界时间。
    Fixed m_staticResetAt;
    /// 本动作是否已施加静止效果。
    bool m_static = false;
    MG_REFLECT(AttackRole, m_action, m_displayed, m_transformLogged, m_staticResetAt, m_static)
};
}
