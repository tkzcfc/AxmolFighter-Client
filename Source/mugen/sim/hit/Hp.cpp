#include "Hit.h"
#include "HitSeams.h"
#include "mugen/sim/World.h"
#include "mugen/sim/status/Status.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::hit
{
namespace
{
// 根据角色身份读取配置，悬空配置引用不强取。
const conf::RoleConfig* roleData(World& world, Entity entity)
{
    const auto* identity = world.ecs().tryGet<RoleIdentity>(entity);
    return identity ? conf::Config::instance().find<conf::RoleConfig>(identity->roleId) : nullptr;
}
// 缺省或负一许可系数等价于一。
int64_t permitValue(const conf::RoleConfig* row, size_t index)
{
    if (!row || index >= row->hitRestrain.size() || row->hitRestrain[index] == -1) return 1;
    return row->hitRestrain[index];
}
// 清空护盾后同步通知其所有者。
void cleanShield(World& world, Entity entity)
{
    world.ecs().get<Vitals>(entity).cleanShield();
    buff::trigger(world, entity, BFEvent::ShieldClean, HitTrigger{});
}
}
// 机关按命中次数死亡，其余角色要求最大生命为正。
bool isDead(World& world, Entity entity)
{
    if (world.ecs().get<RoleIdentity>(entity).roleType == static_cast<int>(EntityRoleType::kMachine))
        return world.ecs().get<RoleHitState>(entity).hitCounts == 0;
    return world.ecs().get<Vitals>(entity).isDead();
}
// 攻击许可与目标对应系数乘积必须严格大于零。
bool checkHurtPermit(World& world, Entity target, Entity effect, HurtPermit permit)
{
    const auto* state = world.ecs().tryGet<EffectState>(effect);
    const auto* attacker = state ? roleData(world, state->m_owner) : nullptr;
    return permitValue(attacker, 0) * permitValue(roleData(world, target), static_cast<size_t>(permit)) > 0;
}
// 护盾比较使用缩放伤害，溢出生命仍使用原伤害加原护盾。
Fixed calcShieldAndHp(World& world, Entity entity, Fixed delta)
{
    auto& vitals = world.ecs().get<Vitals>(entity);
    if (vitals.shieldCovers(delta))
    {
        vitals.absorbShield(delta);
        if (world.hitHooks().m_shieldLayer(world, entity)) cleanShield(world, entity);
        return {};
    }
    const Fixed rest = vitals.absorbShield(delta);
    cleanShield(world, entity);
    return rest;
}
// 治疗修正、护盾和锁血依次生效；死亡事件先于死亡判定和状态转换。
void modifyHp(World& world, Entity entity, Fixed delta, Entity effectEntity, bool noShield)
{
    auto& ecs = world.ecs();
    if (delta < Fixed{} && !ecs.get<Vitals>(entity).m_beHitEnabled) return;
    if (delta > Fixed{})
    {
        const auto modifiers = world.hitHooks().m_modifiers(world, entity);
        if (modifiers.m_chill) return;
        delta = std::max(Fixed{}, delta * ("1"_fx + modifiers.m_healingChange));
    }
    const bool wasDead = isDead(world, entity);
    if (delta >= Fixed{} || checkHurtPermit(world, entity, effectEntity, HurtPermit::Hp))
    {
        if (!noShield && delta < Fixed{}) delta = calcShieldAndHp(world, entity, delta);
        auto& vitals = ecs.get<Vitals>(entity);
        vitals.hp = std::max(vitals.m_hpLock, std::clamp(vitals.hp + delta, Fixed{}, vitals.maxHp));
    }
    HitTrigger context;
    context.m_target = entity;
    context.m_effect = effectEntity;
    if (ecs.get<Vitals>(entity).hp <= Fixed{}) buff::trigger(world, entity, BFEvent::BeforeDeath, context);
    if (!wasDead && isDead(world, entity))
    {
        const auto* row = roleData(world, entity);
        const auto state = ecs.get<RoleStatus>(entity);
        if (!state.isTobeHit() && !state.isDeath())
            status::dealWithStatus(world, entity, EntityRoleStatus::Death, row ? row->deathDisplacementId : -1);
        if (row && row->koEffectId != -1)
        {
            const auto spawned = effect::create(world, entity, row->koEffectId);
            if (spawned.value != 0)
            {
                effect::setCustomPosition(world, spawned, entity);
                effect::enter(world, spawned);
            }
        }
        world.hitHooks().m_ability(world, entity, HitAbilityEvent::Death, context, -1);
    }
    if (!isDead(world, entity)) buff::trigger(world, entity, BFEvent::HpChange, context);
}
}
