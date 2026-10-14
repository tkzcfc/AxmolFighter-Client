#include "HitSeams.h"
#include "mugen/sim/room/RoomEntities.h"

#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffHolder.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/ability/AbilityHolder.h"

namespace mugen::sim
{
// 默认死亡回调转入物品系统，不在命中模块保存掉落状态。
void HitHooks::dropGoods(World& world, Entity entity) { room_entities::drop(world, entity); }
// 按配置目标选择接收池，实体遍历保持世界创建顺序。
void buff::addEntityBuff(World& world, Entity entity, int32_t id,
                         std::optional<int32_t> boundSkill, std::optional<int32_t> origin)
{
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(id);
    if (!config) return;
    if (auto* state = world.ecs().tryGet<RoleHitState>(entity)) state->m_protectionBuffRequests.push_back(id);
    HitBuffApplication request;
    request.m_buffId = Fixed::fromInt(id);
    request.m_origin = origin;
    if (config->target == 1)
    {
        if (config->binding != 0) request.m_boundSkill = boundSkill;
        world.hitHooks().m_apply(world, entity, request);
        return;
    }
    const auto identity = world.ecs().get<RoleIdentity>(entity);
    world.ecs().each<RoleIdentity>([&](Entity target, const RoleIdentity& other) {
        const bool enemy = (identity.hostileCamp & other.camp) != 0;
        const bool selected = config->target == 9 || (config->target == 4 && !enemy) ||
            (config->target == 5 && enemy) || (config->target == 6 && other.roleType == 4) ||
            (config->target == 7 && (other.roleType == 2 || other.roleType == 3));
        if (selected) world.hitHooks().m_apply(world, target, request);
    });
}
// 命中上下文转换为值实例事件，目标分类独立于阵营位掩码。
void HitHooks::poolTrigger(World& world, Entity entity, BFEvent event, const HitTrigger& source)
{
    BuffContext context;
    context.m_target = source.m_target;
    context.m_applicator = source.m_attacker;
    context.m_skillAttribute = source.m_skillAttribute;
    if (source.m_baseSkillId != -1) context.m_currentSkill = source.m_baseSkillId;
    else if (source.m_skillId != -1) context.m_currentSkill = source.m_skillId;
    if (source.m_artifactId != -1) context.m_artifactId = source.m_artifactId;
    if (source.m_artifactIndex != -1) context.m_artifactIndex = source.m_artifactIndex;
    context.m_damage = source.m_damage;
    context.m_critical = source.m_critical;
    buff::BuffHolder(world, entity).dispatchEvent(event, context,
        source.m_target == entity ? 8 : source.m_teammate ? 3 : 2);
}
// 命中特效指定接收池，不再次按配置目标重新分派。
void HitHooks::poolApply(World& world, Entity entity, const HitBuffApplication& application)
{
    if (application.m_buffId != application.m_buffId.floor()) return;
    BuffContext context;
    context.m_applicator = application.m_source.m_attacker;
    context.m_boundSkill = application.m_boundSkill;
    context.m_origin = application.m_origin;
    context.m_skillAttribute = application.m_source.m_skillAttribute;
    if (application.m_source.m_skillId != -1) context.m_frontSkill = application.m_source.m_skillId;
    if (application.m_source.m_artifactId != -1) context.m_artifactId = application.m_source.m_artifactId;
    if (application.m_source.m_artifactIndex != -1) context.m_artifactIndex = application.m_source.m_artifactIndex;
    buff::BuffHolder(world, entity).addBuff(static_cast<int32_t>(application.m_buffId.toInt()), context, world.setup().m_fightType);
}
// 命中保护查询同一个实体实例池，不维护第二份存在性状态。
bool HitHooks::poolHasBuff(World& world, Entity entity, int32_t id) { return buff::BuffHolder(world, entity).hasBuff(id); }
// 行为树叶子名称直接交给 Buff 状态路由。
void HitHooks::poolBehaviorState(World& world, Entity entity, BFEvent event, const char* actionName)
{
    buff::BuffHolder(world, entity).dispatchBehaviorState(event, actionName);
}
// 默认不存在治疗限制。
HpBuffModifiers HitHooks::noModifiers(World&, Entity) { return {}; }
// 默认伤害仍扣生命。
bool HitHooks::noReplacement(World&, Entity, Fixed) { return false; }
// 默认护盾没有层数。
bool HitHooks::noShieldLayer(World&, Entity) { return false; }
// 将命中事件转换为能力池事件。
void HitHooks::noAbility(World& world, Entity entity, HitAbilityEvent event, const HitTrigger& source, int32_t id)
{
    AbilityContext context;
    context.m_target = source.m_target;
    context.m_applicator = source.m_attacker;
    context.m_skillId = source.m_skillId;
    context.m_critical = source.m_critical;
    ability::Event abilityEvent = ability::Event::AfterHit;
    switch (event)
    {
    case HitAbilityEvent::BeforeHit: abilityEvent = ability::Event::BeforeHit; break;
    case HitAbilityEvent::AfterHit: abilityEvent = ability::Event::AfterHit; break;
    case HitAbilityEvent::AfterToBeHit: abilityEvent = ability::Event::AfterToBeHit; break;
    case HitAbilityEvent::SameCamp: abilityEvent = ability::Event::AfterHit; break;
    case HitAbilityEvent::Anger: abilityEvent = ability::Event::AfterHit; break;
    case HitAbilityEvent::Death: abilityEvent = ability::Event::AfterToBeHit; break;
    case HitAbilityEvent::Crystal: abilityEvent = ability::Event::AfterToBeHit; break;
    case HitAbilityEvent::Obstacle: abilityEvent = ability::Event::AfterHit; break;
    }
    if (id > 0) ability::AbilityHolder(world, entity).triggerId(id, abilityEvent, context);
    else ability::AbilityHolder(world, entity).trigger(abilityEvent, context);
}
// 将 Buff 上下文转换为能力事件，保留技能绑定和目标归属。
void HitHooks::boundAbility(World& world, Entity entity, int32_t id, BFEvent event, const BuffContext& source)
{
    AbilityContext context;
    context.m_target = source.m_target;
    context.m_applicator = source.m_applicator;
    context.m_skillId = source.m_currentSkill.value_or(-1);
    context.m_critical = source.m_critical;
    ability::Event abilityEvent = ability::Event::InitAbility;
    if (event == BFEvent::BeforeCastSkill) abilityEvent = ability::Event::BeforeCastSkill;
    else if (event == BFEvent::AfterCastSkill) abilityEvent = ability::Event::AfterCastSkill;
    else if (event == BFEvent::BeforeHit) abilityEvent = ability::Event::BeforeHit;
    else if (event == BFEvent::AfterHit) abilityEvent = ability::Event::AfterHit;
    else if (event == BFEvent::BeforeToBeHit) abilityEvent = ability::Event::BeforeToBeHit;
    else if (event == BFEvent::AfterToBeHit) abilityEvent = ability::Event::AfterToBeHit;
    ability::AbilityHolder(world, entity).triggerId(id, abilityEvent, context);
}
// 复活清空旧状态，死亡则同步卸载能力池。
void HitHooks::roleAbilities(World& world, Entity entity, bool reload)
{
    ability::AbilityHolder(world, entity).clear();
    if (reload) return;
}
// 未配置技能属性时使用角色等级。
Fixed HitHooks::roleDamageLevel(World& world, Entity entity, Entity)
{
    return Fixed::fromInt(world.ecs().get<RoleIdentity>(entity).level);
}
// 同步分派给世界实例的 Buff 接缝，允许在返回前执行免死。
void buff::trigger(World& world, Entity entity, BFEvent event, const HitTrigger& context)
{
    world.hitHooks().m_trigger(world, entity, event, context);
}
// 转交施加请求，不在命中层建立 Buff 池。
void buff::dealBuff(World& world, Entity entity, const HitBuffApplication& application)
{
    world.hitHooks().m_apply(world, entity, application);
}
}
