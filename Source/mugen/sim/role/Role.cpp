#include "Role.h"
#include "mugen/sim/World.h"
#include "mugen/sim/effect/Effect.h"
#include "mugen/sim/hit/DamageCalculator.h"
#include "mugen/sim/hit/HitSeams.h"
#include "mugen/sim/trace/TraceSink.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim
{
namespace
{
// 根据角色身份读取配置，悬空配置引用不强取。
const conf::RoleConfig* roleData(World& world, Entity entity)
{
    const auto* identity = world.ecs().tryGet<RoleIdentity>(entity);
    return identity ? conf::Config::instance().find<conf::RoleConfig>(identity->roleId) : nullptr;
}
}  // namespace

Role::Role(World& world, Entity entity) : m_world(world), m_entity(entity) {}

Entity Role::getEntity() const { return m_entity; }
RoleStatus& Role::getStatus() const { return m_world.ecs().get<RoleStatus>(m_entity); }
Vitals& Role::getVitals() const { return m_world.ecs().get<Vitals>(m_entity); }
RoleHitState& Role::getHitState() const { return m_world.ecs().get<RoleHitState>(m_entity); }
RoleIdentity& Role::getIdentity() const { return m_world.ecs().get<RoleIdentity>(m_entity); }

// 机关按命中次数死亡，其余角色要求最大生命为正。
bool Role::isDead() const
{
    if (m_world.ecs().get<RoleIdentity>(m_entity).roleType == static_cast<int>(EntityRoleType::kMachine))
        return m_world.ecs().get<RoleHitState>(m_entity).hitCounts == 0;
    return m_world.ecs().get<Vitals>(m_entity).isDead();
}

// 护盾比较使用缩放伤害，溢出生命仍使用原伤害加原护盾。
Fixed Role::_absorbShield(Fixed delta)
{
    auto& vitals = m_world.ecs().get<Vitals>(m_entity);
    if (vitals.shieldCovers(delta))
    {
        vitals.absorbShield(delta);
        if (m_world.hitHooks().m_shieldLayer(m_world, m_entity)) _cleanShield();
        return {};
    }
    const Fixed rest = vitals.absorbShield(delta);
    _cleanShield();
    return rest;
}

void Role::_cleanShield()
{
    m_world.ecs().get<Vitals>(m_entity).cleanShield();
    buff::trigger(m_world, m_entity, BFEvent::ShieldClean, HitTrigger{});
}

// 治疗修正、护盾和锁血依次生效；死亡事件先于死亡判定和状态转换。
void Role::modifyHp(Fixed delta, Entity effect, bool noShield)
{
    auto& ecs = m_world.ecs();
    if (delta < Fixed{} && !ecs.get<Vitals>(m_entity).m_beHitEnabled) return;
    if (delta > Fixed{})
    {
        const auto modifiers = m_world.hitHooks().m_modifiers(m_world, m_entity);
        if (modifiers.m_chill) return;
        delta = std::max(Fixed{}, delta * ("1"_fx + modifiers.m_healingChange));
    }
    const bool wasDead = isDead();
    if (delta >= Fixed{} || hit::DamageCalculator(m_world).checkHurtPermit(m_entity, effect, hit::HurtPermit::Hp))
    {
        if (!noShield && delta < Fixed{}) delta = _absorbShield(delta);
        auto& vitals = ecs.get<Vitals>(m_entity);
        vitals.hp = std::max(vitals.m_hpLock, std::clamp(vitals.hp + delta, Fixed{}, vitals.maxHp));
    }
    HitTrigger context;
    context.m_target = m_entity;
    context.m_effect = effect;
    if (ecs.get<Vitals>(m_entity).hp <= Fixed{}) buff::trigger(m_world, m_entity, BFEvent::BeforeDeath, context);
    if (!wasDead && isDead())
    {
        const auto* row   = roleData(m_world, m_entity);
        const auto state  = ecs.get<RoleStatus>(m_entity);
        if (!state.isTobeHit() && !state.isDeath())
            changeStatus(EntityRoleStatus::Death, row ? row->deathDisplacementId : -1);
        if (row && row->koEffectId != -1)
        {
            const auto spawned = effect::EffectActor::create(m_world, m_entity, row->koEffectId);
            if (spawned.value != 0)
            {
                effect::EffectActor(m_world, spawned).setCustomPosition(m_entity);
                effect::EffectActor(m_world, spawned).enter();
            }
        }
        m_world.hitHooks().m_ability(m_world, m_entity, HitAbilityEvent::Death, context, -1);
    }
    if (!isDead()) buff::trigger(m_world, m_entity, BFEvent::HpChange, context);
}

// 按状态机规则应用转换，死亡角色拒绝后续转换。
bool Role::changeStatus(EntityRoleStatus bits, int32_t id, FixedVec3 movePos)
{
    auto& role = m_world.ecs().get<RoleStatus>(m_entity);
    if (role.isDeath())
        return false;
    using S = EntityRoleStatus;
    switch (bits)
    {
    case S::Hit:
        role.eEntityStatus                = S::Idle | S::Hit;
        role.entityParams.hit_id          = id;
        role.entityParams.hit_counts      = 1;
        m_world.ecs().get<RoleHitState>(m_entity).hitCounts = 1;
        m_world.ecs().get<RoleHitState>(m_entity).forceVelocity = {};
        initHitProtect();
        break;
    case S::Death:
        role.eEntityStatus                      = S::Death;
        role.entityParams.death_displacement_id = id;
        trace::noteDeath(m_world, m_entity);
        break;
    case S::Jostled:
        role.entityParams.displacement_id = id;
        role.mix(bits);
        break;
    case S::Patrol:
    case S::Finding:
    case S::Chase:
    case S::Alert:
        role.entityParams.move_pos = movePos;
        if (role.eEntityStatus != S::Idle)
            return false;
        role.mix(bits);
        break;
    case S::Walk:
    case S::Run:
        role.abort(S::Finding);
        if (role.eEntityStatus != S::Idle)
            return false;
        role.mix(bits);
        break;
    case S::Idle:
        if (!role.isMove() || role.isAttack())
            return false;
        role.eEntityStatus = S::Idle;
        break;
    default:
        role.mix(bits);
        break;
    }
    return true;
}

// 配置总值与运行剩余值分别参与保护判断。
bool Role::isRigidity() const
{
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(m_world.ecs().get<RoleIdentity>(m_entity).roleId);
    return row && m_world.ecs().get<RoleHitState>(m_entity).isRigidity(Fixed::fromInt(row->rigidity));
}

// 保留读取本身扣减硬直的语义。
Fixed Role::getStiffTime()
{
    const auto* row = conf::Config::instance().find<conf::RoleConfig>(m_world.ecs().get<RoleIdentity>(m_entity).roleId);
    return m_world.ecs().get<RoleHitState>(m_entity).getStiffTime(
        row ? Fixed::fromInt(row->rigidity) : Fixed{}, isDead());
}

// 耗尽后每次追击都加重下落，保护提示只向 Buff 接缝发送请求。
bool Role::applyWeight(Fixed amount, bool hitProtect)
{
    auto& state = m_world.ecs().get<RoleHitState>(m_entity);
    state.rigidity = std::max(Fixed{}, state.rigidity - amount);
    if (isDead() || !isRigidity()) return false;
    state.forceVelocity.y -= state.weight * Fixed::fromInt(state.hitCounts);
    if (m_world.ecs().get<RoleIdentity>(m_entity).roleType == 1)
    {
        const auto query = m_world.hitHooks().m_hasBuff;
        if (!query || (!query(m_world, m_entity, 155) && !query(m_world, m_entity, 173)))
        {
            buff::addEntityBuff(m_world, m_entity, hitProtect ? 173 : 155);
            buff::addEntityBuff(m_world, m_entity, 154);
        }
    }
    return true;
}

// 首次命中记录当前生命减去最大生命比例后的阈值。
void Role::initHitProtect()
{
    auto& state       = m_world.ecs().get<RoleHitState>(m_entity);
    const auto& vitals = m_world.ecs().get<Vitals>(m_entity);
    state.initHitProtect(m_world.setup().m_pvp, vitals.hp, vitals.maxHp, conf::kHitProtectHpScale,
                         conf::kHitProtectCleanTime);
}

// 已执行的保护不再因追击延长。
void Role::refreshHitProtect()
{
    m_world.ecs().get<RoleHitState>(m_entity).updateHitProtect(conf::kHitProtectCleanTime);
}

// 清除本轮保护状态，下一次命中可重新建立阈值。
void Role::resetHitProtect()
{
    m_world.ecs().get<RoleHitState>(m_entity).resetHitProtect();
}

// 先处理超时，再检测血量；仅受击中的角色强制耗尽浮空值。
void Role::updateHitProtect()
{
    auto& state = m_world.ecs().get<RoleHitState>(m_entity);
    if (!state.m_hitProtectHp || state.m_hitProtectExecuted) return;
    state.m_hitProtectTime -= m_world.ecs().singleton<WorldClock>().dtMs;
    if (state.m_hitProtectTime <= Fixed{}) { state.resetHitProtect(); return; }
    if (m_world.ecs().get<Vitals>(m_entity).hp <= *state.m_hitProtectHp)
    {
        if (m_world.ecs().get<RoleStatus>(m_entity).isTobeHit())
            applyWeight(state.rigidity, true);
        state.m_hitProtectExecuted = true;
    }
}
}  // namespace mugen::sim
