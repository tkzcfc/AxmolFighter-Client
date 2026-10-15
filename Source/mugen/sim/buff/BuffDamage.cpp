#include "BuffHolder.h"
#include "mugen/sim/World.h"
#include "mugen/sim/hit/DamageCalculator.h"
#include "mugen/sim/role/Role.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::buff
{
// 先组合倍率再乘标准伤害，负数同样向下取整。
Fixed BuffHolder::calculateBuffDamage(Fixed standardHurt, Fixed parameter, Fixed addition)
{
    return (standardHurt * (parameter * ("1"_fx + addition))).floor();
}

// 缺失施加者时采用本实体，技能等级通过既有属性来源接缝解析。
Fixed BuffHolder::getAttackValue(const BuffInstance& instance) const
{
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(instance.m_id);
    if (!config || config->paramValue.empty()) return {};
    const auto source = m_world.ecs().tryGet<RoleIdentity>(instance.m_context.m_applicator) ?
        instance.m_context.m_applicator : m_entity;
    Fixed level = Fixed::fromInt(m_world.ecs().get<RoleIdentity>(source).level);
    if (config->hurtType == 1 && instance.m_context.m_skillAttribute.value != 0)
        level = m_world.hitHooks().m_damageLevel(m_world, source, instance.m_context.m_skillAttribute);
    const Fixed addition = std::max(Fixed{}, level) * conf::kBuffHurtDefaultAddition;
    return calculateBuffDamage(hit::DamageCalculator(m_world).standardHurt(source), config->paramValue.front(), addition)
        * Fixed::fromInt(instance.m_lifecycle.m_repeat);
}

// 生命变化沿用护盾和死亡事件管线，伤害统计记录原始带符号伤害的相反数。
void BuffHolder::dealBuffDamage(Entity attacker, Fixed attackValue)
{
    if (auto* state = m_world.ecs().tryGet<RoleHitState>(attacker)) state->m_dps -= attackValue;
    Role(m_world, m_entity).modifyHp(attackValue);
    m_world.emit(m_entity, HurtNumber{(-attackValue).toInt()});
}
}
