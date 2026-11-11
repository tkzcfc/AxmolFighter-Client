#pragma once
#include "mugen/sim/Events.h"
#include "mugen/core/math/Fixed.h"

namespace mugen::conf::battle
{
struct HitDef;
struct EffectDef;
}  // namespace mugen::conf::battle

namespace mugen::sim
{
class World;
namespace combat::rules
{
// 一次命中微管线的值上下文（§6.6）。步骤间只传这份值，不跨系统调用。
struct HitContext
{
    // 触发本次管线的接触。
    ContactEvent m_contact;
    // 攻击特效。
    Entity m_effect;
    // 受击目标。
    Entity m_target;
    // 属性来源（特效主人）。
    Entity m_attacker;
    // 特效配置标识。
    int32_t m_effectId = 0;
    // 来源技能与槽首技能。
    int32_t m_skillId = -1;
    int32_t m_baseSkillId = -1;
    // 技能槽位下标。
    int32_t m_slot = 0;
    // 命中配置（进程级只读）。
    const conf::battle::HitDef* m_hit = nullptr;
    // 特效配置（进程级只读）。
    const conf::battle::EffectDef* m_effectRow = nullptr;
    // 本次伤害（取整后）。
    Fixed m_damage;
    // 是否暴击。
    bool m_critical = false;
};

// 步骤 1：门控。通过时填好上下文。
bool gate(World& world, const ContactEvent& contact, HitContext& ctx);
// 步骤 2：更新 HitTracker（次数、间隔、目标去重）。
void recordHit(World& world, const HitContext& ctx);
// 帧末提交命中间隔（本帧命中后从下一帧起进入间隔冷却）。
void commitHitInterval(World& world, Entity effect);
// 步骤 5：属性闪避。成功则发出 Miss 并结束本次管线。
bool resolveDodge(World& world, const HitContext& ctx);
// 步骤 8：伤害公式。抽样顺序为 Crit 再 Float。
void computeDamage(World& world, HitContext& ctx);
// 步骤 9：经 health::rules 扣血。
void applyDamage(World& world, const HitContext& ctx);
// 步骤 10：写攻击者与特效顿帧，受击者顿帧进入延迟表。
void applyHitStop(World& world, const HitContext& ctx);
// 步骤 10：朝向、硬直、受击请求；非受击族则强制打断施法。
void requestHitReaction(World& world, const HitContext& ctx);
// 步骤 14：受击特效 SpawnRequest。
void spawnHitEffects(World& world, const HitContext& ctx);
// 步骤 15：发出命中事实。
void emitHit(World& world, const HitContext& ctx);
}  // namespace combat::rules
}  // namespace mugen::sim
