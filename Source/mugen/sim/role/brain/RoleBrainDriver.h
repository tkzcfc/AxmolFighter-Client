#pragma once

#include "RoleBrain.h"
#include "mugen/sim/motion/Displacement.h"

namespace mugen::sim::role::brain
{
// 角色脑的临时驱动视图，不存入组件、不拥有资源，生命周期限于单次调用。
class RoleBrainDriver
{
public:
    // 构造指向指定世界、实体与脑状态的视图。
    RoleBrainDriver(World& world, Entity entity, RoleBrain& brain, bool city)
        : m_world(world), m_entity(entity), m_brain(brain), m_city(city) {}
    // 进入第一个可进入的分支；无可进入则退回待机并记录一次错误。
    bool enterFirst();
    // 推进一帧；NotEntered 时先进入再更新（房间切换首帧语义）。
    void update();
    // 退出当前分支（先叶子后守卫）。
    void exitBranch();
    // 把位移事件分派到当前分支的叶子。
    bool onDisplacement(DisplacementEvent event);
    // 返回当前分支对应的叶子动作。
    behavior::ActionType currentAction() const;

private:
    // 分支对应的动作类型（用于叶子分发）。
    behavior::ActionType _branchAction(RoleBranch branch) const;
    // 分支守卫是否成立（按分支状态位与附加条件判断）。
    bool _guardHolds(RoleBranch branch) const;
    // 退出守卫的副作用（攻击复位技能，非待机/复活状态清除状态位）。
    void _exitGuard(RoleBranch branch);
    // 按分支重置叶子状态数据为对应类型。
    void _resetData();
    // 进入当前分支的叶子；返回是否成功（仅 Attack 可能失败）。
    bool _enterState();
    // 推进当前分支的叶子；Death 分支按两段（Death 后 Destroy）推进。
    StateStep _updateState();
    // 退出当前分支的叶子。
    void _exitState();
    // 尝试进入指定分支；守卫失败不触发任何退出调用，进入失败（仅 Attack）补一次守卫退出。
    bool _tryEnter(RoleBranch branch);
    // 无可进入分支时退回待机并只记录一次错误日志。
    bool _fallbackToIdle();

    // 本场战斗世界。
    World& m_world;
    // 视图绑定的角色实体。
    Entity m_entity{};
    // 该实体的脑状态。
    RoleBrain& m_brain;
    // 是否按城镇模板选择分支。
    bool m_city = false;
};
}  // namespace mugen::sim::role::brain
