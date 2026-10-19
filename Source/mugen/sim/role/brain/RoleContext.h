#pragma once

#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/sim/World.h"
#include "mugen/sim/input/PlayerCommand.h"
#include "mugen/sim/motion/Displacement.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/role/RoleComponents.h"
#include "mugen/sim/role/brain/RoleAction.h"

namespace mugen::sim
{
class World;
}

namespace mugen::sim::role::brain
{
// 角色行为的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class RoleContext
{
public:
    RoleContext(World& world, Entity entity) : m_world(world), m_entity(entity) {}

    // 读取实体的指定组件。
    template <class T>
    T& get()
    {
        return m_world.ecs().get<T>(m_entity);
    }
    World& world() { return m_world; }
    Entity entity() const { return m_entity; }
    // 当前逻辑帧毫秒时长。
    Fixed dtMs() const { return m_world.ecs().singleton<WorldClock>().dtMs; }
    // 返回指向自身实体的轻量角色视图。
    Role role() const { return Role(m_world, m_entity); }
    // 是否为英雄角色。
    bool isHero() const { return role().getIdentity().roleType == 1; }
    // 当前行为树节点标识，用于位移监听登记。
    int32_t node() const { return m_node; }
    // 当前分派的位移事件。
    DisplacementEvent event() const { return m_event; }

    // 由桥接写入。
    void setNode(int32_t node) { m_node = node; }
    void setEvent(DisplacementEvent event) { m_event = event; }

    // 通知普通动作叶子开始。
    void notifyActionStart(behavior::ActionType type);
    // 通知普通动作叶子结束。
    void notifyActionEnd(behavior::ActionType type);
    // 消费本帧控制命令并写入 RoleControl.m_last。
    void consumeControl();
    // 分派位移事件到当前分支的叶子。
    bool onDisplacementEvent(DisplacementEvent event);

private:
    // 将技能控制命令转换为方向角并交给技能系统。
    void _skillEvent(const PlayerCommand& command);

    World& m_world;
    Entity m_entity;
    int32_t m_node            = -1;
    DisplacementEvent m_event = DisplacementEvent::Brake;
};
}  // namespace mugen::sim::role::brain
