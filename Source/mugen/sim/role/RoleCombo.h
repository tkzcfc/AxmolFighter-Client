#pragma once
#include "RoleComponents.h"
#include "mugen/core/ecs/Entity.h"

namespace mugen::sim
{
class World;
namespace role
{
// 连击与评价系统的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
// 连击数、评价等级与经验等状态保存在 RoleHitState，评价等级同步写入 BuffExternalConditions。
class RoleCombo
{
public:
    // 构造指向指定世界实例中某个实体的视图。
    RoleCombo(World& world, Entity entity);
    // 推进连击超时与评价经验衰减。
    void update();
    // 命中后累加连击数与评价经验，并结算击杀、眩晕与首领倍率。
    void add(Fixed exp, Entity target);
    // 清空全部连击与评价状态，恢复默认经验倍率。
    void reset();

private:
    // 根据当前评价经验重算评价等级。
    void recomputeEvaluate(RoleHitState& state);
    // 将当前评价等级同步到 Buff 条件输入。
    void syncEvaluate(RoleHitState& state);

    World& m_world;
    Entity m_entity{};
};
}  // namespace role
}  // namespace mugen::sim
