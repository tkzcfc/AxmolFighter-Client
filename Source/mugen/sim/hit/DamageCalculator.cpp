#include "DamageCalculator.h"
#include "Damage.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

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
    if (!row || index >= row->hitRestrain.size() || row->hitRestrain[index] == -1)
        return 1;
    return row->hitRestrain[index];
}
}  // namespace

DamageCalculator::DamageCalculator(World& world) : m_world(world) {}

// 攻击许可与目标对应系数乘积必须严格大于零。
bool DamageCalculator::checkHurtPermit(Entity target, Entity effect, HurtPermit permit) const
{
    const auto* state    = m_world.ecs().tryGet<EffectState>(effect);
    const auto* attacker = state ? roleData(m_world, state->m_owner) : nullptr;
    return permitValue(attacker, 0) * permitValue(roleData(m_world, target), static_cast<size_t>(permit)) > 0;
}
// 使用实体句柄解析最终主人，循环归属不参与伤害计算。
Entity DamageCalculator::finalOwner(Entity entity) const
{
    std::vector<Entity> visited;
    while (const auto* identity = m_world.ecs().tryGet<RoleIdentity>(entity))
    {
        if (std::find(visited.begin(), visited.end(), entity) != visited.end())
        {
            MG_LOG_E("Cyclic damage ownership at entity {}", entity.value);
            MG_ASSERT(false && "Cyclic damage ownership");
            return {};
        }
        visited.push_back(entity);
        if (identity->roleType != static_cast<int>(EntityRoleType::kSummon))
            return entity;
        if (identity->m_owner == entity)
            return {};
        entity = identity->m_owner;
    }
    return {};
}
// 英雄使用物攻加等级标准，其他单位使用基础伤害加十级分段标准。
Fixed DamageCalculator::standardHurt(Entity attacker) const
{
    const auto owner     = finalOwner(attacker);
    const auto* identity = m_world.ecs().tryGet<RoleIdentity>(owner);
    if (!identity)
        return {};
    const auto& attributes = m_world.ecs().get<Attributes>(owner);
    const bool hero        = identity->roleType == static_cast<int>(EntityRoleType::kHero);
    const int level        = hero ? identity->level : std::max(1, identity->level / 10);
    const auto* row        = conf::Config::instance().find<conf::SkillHurtConfig>(level);
    if (!row)
    {
        MG_LOG_W("Missing standard damage configuration {}", level);
        return {};
    }
    return attributes.value(hero ? 5 : 15) + Fixed::fromInt(hero ? row->hurt : row->monsterHurt);
}
// 从真实打击配置读取输入，属性来自攻击者而非特效的中立身份。
bool DamageCalculator::damageInput(Entity effect, Entity target, DamageInput& input) const
{
    const auto source     = m_world.ecs().get<EffectState>(effect);
    const auto* effectRow = conf::Config::instance().find<conf::EffectConfig>(source.m_configId);
    if (!effectRow || effectRow->hitId == -1)
        return false;
    const auto* hitRow  = conf::Config::instance().find<conf::SkillHitConfig>(effectRow->hitId);
    const auto* attack  = m_world.ecs().tryGet<Attributes>(source.m_owner);
    const auto* defense = m_world.ecs().tryGet<Attributes>(target);
    if (!hitRow || !attack || !defense)
        return false;
    input.m_attacker     = *attack;
    input.m_target       = *defense;
    input.m_hit          = *hitRow;
    input.m_standardHurt = standardHurt(source.m_owner);
    input.m_addition     = m_world.hitHooks().m_damageLevel(m_world, source.m_owner, source.m_skillAttributeTag) *
                       conf::kSkillHurtDefaultAddition;
    input.m_artifact = source.m_artifactId != -1 || source.m_capabilityTrigger;
    return true;
}
}  // namespace mugen::sim::hit
