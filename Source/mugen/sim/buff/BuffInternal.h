#pragma once
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/World.h"
#include "mugen/conf/battle/AbilityDef.h"

namespace mugen::sim::buff::detail
{
// 一次 op 列表的执行环境。寄存器只在这一次列表内有效。
struct Exec
{
    // 世界。
    World* m_world = nullptr;
    // Buff 持有者。
    Entity m_self;
    // 当前实例序号。执行中可能被删，用序号回查。
    uint32_t m_serial = 0;
    // 当前钩子位。
    int32_t m_hook = 0;
    // 这一次列表的寄存器。
    Fixed m_reg;
    // 是否刚把门闩合上。
    bool m_justLatched = false;
    // 来自特殊能力。
    bool m_fromAbility = false;
};

// 按序号查找实例。
BuffInstance* findInstance(World& world, Entity entity, uint32_t serial);
// 读取配置。
const conf::battle::BuffDef* buffDef(int32_t id);
// 求值。
Fixed evalValue(Exec& exec, const conf::battle::ValueExpr& expr);
// 谓词。种类 None 为真。
bool evalPredicate(Exec& exec, const conf::battle::Predicate& pred);
// 执行一条 op。
void execOp(Exec& exec, const conf::battle::BuffOp& op);
// 执行一组瞬时 op。
void runOps(Exec& exec, const std::vector<conf::battle::BuffOp>& ops);
// 按钩子激活、执行、撤销。
void onHook(World& world, Entity entity, uint32_t serial, int32_t hook);
// 按来源撤销常驻效果。removeKids 为假时留下子 Buff（施加器退出钩子不撤子效果）。
void revoke(World& world, Entity entity, uint32_t serial, bool removeKids = true);
// 逻辑删除一个实例。
void destroySerial(World& world, Entity entity, uint32_t serial);
// 特殊能力的一次触发。
void triggerAbility(World& world, Entity entity, AbilityInstance& ability, const conf::battle::AbilityDef& def);
// 标准伤害（根实体）。
Fixed standardHurt(World& world, Entity attacker);
}  // namespace mugen::sim::buff::detail
