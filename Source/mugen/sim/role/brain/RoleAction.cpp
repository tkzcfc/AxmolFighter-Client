#include "RoleAction.h"

#include "mugen/sim/World.h"
#include "mugen/conf/GameDef.h"
#include <array>

namespace mugen::sim::behavior
{
namespace
{
// 生命周期匹配使用动作类名，不使用可共享的动画名。
const char* behaviorName(ActionType type)
{
    constexpr std::array names{"ActionBase",     "ActionDeath",    "ActionGetUp",     "ActionHitFloor", "ActionHitDown",
                               "ActionHitUp",    "ActionHitSwitch", "ActionHit",      "ActionFall",     "ActionPatrol",
                               "ActionChase",    "ActionAlert",     "ActionRun",      "ActionWalk",     "ActionIdle",
                               "ActionSpurt",    "ActionCollect",   "ActionActive",   "ActionTransfer", "ActionMove",
                               "ActionReadyIdle", "ActionReadyMove", "ActionJostled", "ActionFollow",   "ActionSpecIdle",
                               "ActionControlled", "ActionWake",     "ActionRevive",    "ActionDestroy"};
    return names.at(static_cast<size_t>(type));
}
}  // namespace

// 查表取得普通动作的动画名称。
const char* animationName(ActionType type)
{
    constexpr std::array names{"",        "death",   "getup",   "toground", "flydown",  "flyup", "hitswitch",
                               "hit",     "flydown", "running", "running",  "running",  "walk",  "running",
                               "stand",   "spurt",   "death",   "active",   "transfer", "walk",  "running",
                               "running", "running", "running", "special",  "hit", "stand", "stand", ""};
    return names.at(static_cast<size_t>(type));
}

// 将普通动作开始通知转交给世界拥有的 Buff 接缝。
void behaviorStateStart(World& world, Entity entity, ActionType type)
{
    if (world.hitHooks().m_behaviorState)
        world.hitHooks().m_behaviorState(world, entity, BFEvent::BehaviorStateStart, behaviorName(type));
}

// 将普通动作结束通知转交给世界拥有的 Buff 接缝。
void behaviorStateEnd(World& world, Entity entity, ActionType type)
{
    if (world.hitHooks().m_behaviorState)
        world.hitHooks().m_behaviorState(world, entity, BFEvent::BehaviorStateEnd, behaviorName(type));
}
}  // namespace mugen::sim::behavior
