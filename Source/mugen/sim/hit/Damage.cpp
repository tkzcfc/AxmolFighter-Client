#include "Hit.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::hit
{
// 使用实体句柄解析最终主人，循环归属不参与伤害计算。
Entity finalOwner(World& world, Entity entity)
{
    std::vector<Entity> visited;
    while (const auto* identity = world.ecs().tryGet<RoleIdentity>(entity))
    {
        if (std::find(visited.begin(), visited.end(), entity) != visited.end())
        {
            MG_LOG_E("Cyclic damage ownership at entity {}", entity.value);
            MG_ASSERT(false && "Cyclic damage ownership");
            return {};
        }
        visited.push_back(entity);
        if (identity->roleType != static_cast<int>(EntityRoleType::kSummon)) return entity;
        if (identity->m_owner == entity) return {};
        entity = identity->m_owner;
    }
    return {};
}
// 英雄使用物攻加等级标准，其他单位使用基础伤害加十级分段标准。
Fixed standardHurt(World& world, Entity attacker)
{
    const auto owner = finalOwner(world, attacker);
    const auto* identity = world.ecs().tryGet<RoleIdentity>(owner);
    if (!identity) return {};
    const auto& attributes = world.ecs().get<Attributes>(owner);
    const bool hero = identity->roleType == static_cast<int>(EntityRoleType::kHero);
    const int level = hero ? identity->level : std::max(1, identity->level / 10);
    const auto* row = conf::Config::instance().find<conf::SkillHurtConfig>(level);
    if (!row)
    {
        MG_LOG_W("Missing standard damage configuration {}", level);
        return {};
    }
    return attributes.value(hero ? 5 : 15) + Fixed::fromInt(hero ? row->hurt : row->monsterHurt);
}
// 从真实打击配置读取输入，属性来自攻击者而非特效的中立身份。
bool damageInput(World& world, Entity effect, Entity target, DamageInput& input)
{
    const auto source = world.ecs().get<EffectState>(effect);
    const auto* effectRow = conf::Config::instance().find<conf::EffectConfig>(source.m_configId);
    if (!effectRow || effectRow->hitId == -1) return false;
    const auto* hitRow = conf::Config::instance().find<conf::SkillHitConfig>(effectRow->hitId);
    const auto* attack = world.ecs().tryGet<Attributes>(source.m_owner);
    const auto* defense = world.ecs().tryGet<Attributes>(target);
    if (!hitRow || !attack || !defense) return false;
    input.m_attacker = *attack;
    input.m_target = *defense;
    input.m_hit = *hitRow;
    input.m_standardHurt = standardHurt(world, source.m_owner);
    input.m_addition = world.hitHooks().m_damageLevel(world, source.m_owner, source.m_skillAttributeTag) * conf::kSkillHurtDefaultAddition;
    input.m_artifact = source.m_artifactId != -1 || source.m_capabilityTrigger;
    return true;
}
}
