#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/math/Fixed.h"

namespace mugen::sim
{
/// 技能触发接口依赖的战斗世界。
class World;
/// 传递给技能相关事件的来源与数值。
struct SkillTrigger
{
    /// 触发事件的技能数组下标。
    int32_t m_skill = -1;
    /// 事件携带的资源数值。
    Fixed m_amount{};
    /// 关联动作配置 id。
    int32_t m_actionId = -1;
};
namespace ability
{
    /// 技能施放生命周期中的能力事件类型。
    enum class SkillEvent {
        /// 在技能正式施放前触发。
        BeforeCastSkill,
        /// 在技能施放结束后触发。
        AfterCastSkill
    };
}
}
