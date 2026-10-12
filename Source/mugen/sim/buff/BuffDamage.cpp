#include "BuffDamage.h"
#include "mugen/sim/World.h"
#include "mugen/sim/hit/Hit.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::buff
{
// 先组合倍率再乘标准伤害，负数同样向下取整。
Fixed calculateBuffDamage(Fixed standardHurt, Fixed parameter, Fixed addition)
{
    return (standardHurt * (parameter * ("1"_fx + addition))).floor();
}

// 缺失施加者时采用本实体，技能等级通过既有属性来源接缝解析。
Fixed getAttackValue(World& world, Entity entity, const BuffInstance& instance)
{
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(instance.m_id);
    if (!config || config->paramValue.empty()) return {};
    const auto source = world.ecs().tryGet<RoleIdentity>(instance.m_context.m_applicator) ?
        instance.m_context.m_applicator : entity;
    Fixed level = Fixed::fromInt(world.ecs().get<RoleIdentity>(source).level);
    if (config->hurtType == 1 && instance.m_context.m_skillAttribute.value != 0)
        level = world.hitHooks().m_damageLevel(world, source, instance.m_context.m_skillAttribute);
    const Fixed addition = std::max(Fixed{}, level) * conf::kBuffHurtDefaultAddition;
    return calculateBuffDamage(hit::standardHurt(world, source), config->paramValue.front(), addition)
        * Fixed::fromInt(instance.m_lifecycle.m_repeat);
}

// 生命变化沿用护盾和死亡事件管线，伤害统计记录原始带符号伤害的相反数。
void dealBuffDamage(World& world, Entity entity, Entity attacker, Fixed attackValue)
{
    if (auto* state = world.ecs().tryGet<RoleHitState>(attacker)) state->m_dps -= attackValue;
    hit::modifyHp(world, entity, attackValue);
    world.emit(entity, HurtNumber{(-attackValue).toInt()});
}
}
