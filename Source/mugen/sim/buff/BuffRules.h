#pragma once
#include "mugen/conf/BuffRules.h"
#include "BuffComponents.h"

namespace mugen::sim
{
class World;
namespace buff
{
// 五种生命周期回调的稳定索引。
enum class RuleHook : uint8_t
{
    Enter,
    Exit,
    Update,
    Begin,
    End
};
// 规则调用使用实例序号，每次回调后由调用者重新查找组件。
using RuleFunction = void (*)(World&, Entity, uint64_t, Fixed);
// 不可变规则分派项，无运行时闭包或战斗状态。
struct RuleDispatch
{
    // 进入、退出、周期、开始事件和结束事件回调。
    std::array<RuleFunction, 5> m_hooks;
};
// 按稠密配置规则标识取得只读分派表项。
const RuleDispatch& ruleDispatch(conf::BuffRuleId rule);
// 判断规则是否仍使用未移植占位分派。
bool isUnportedRule(conf::BuffRuleId rule);
// 按稳定序号重新定位实例，不跨回调保留返回的指针。
BuffInstance* findInstance(World& world, Entity entity, uint64_t serial);
// 返回目标当前最高嘲讽权重对应的施加者。
Entity sneerTarget(World& world, Entity entity);
// 判断角色是否处于持续移动状态。
bool isMoved(World& world, Entity entity);
}  // namespace buff
}  // namespace mugen::sim
