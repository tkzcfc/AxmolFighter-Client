#include "BuffRules.h"
#include "BuffCondition.h"
#include "BuffDamage.h"
#include "BuffPool.h"
#include "BuffHolder.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/hit/HitSeams.h"
#include "mugen/sim/skill/SkillCaster.h"
#include "mugen/sim/skill/SkillComponents.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::buff
{
// 在实体池中按序号查找，不使用可能因扩容失效的地址作为身份。
BuffInstance* BuffHolder::findInstance(uint64_t serial)
{
    auto* pool = m_world.ecs().tryGet<BuffPool>(m_entity);
    if (!pool)
        return nullptr;
    for (auto& instance : pool->m_instances)
        if (instance.m_serial == serial)
            return &instance;
    return nullptr;
}
namespace
{
// 已移植规则未覆盖的钩子保持基类空行为。
void noHook(World&, Entity, uint64_t, Fixed) {}

// 规则播放音效只发送表现事件，不调用音频后端。
void playEffect(World& world, Entity entity, uint64_t serial)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto* row = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
    if (row && row->audioId != -1)
    {
        Sound sound;
        sound.id = row->audioId;
        world.emit(entity, sound);
    }
}

// 尚未移植的规则按场次和配置标识只报告一次。
void unported(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const int32_t id = instance->m_id;
    auto& logged     = world.ecs().singleton<RoomState>().m_unportedBuffIds;
    if (std::find(logged.begin(), logged.end(), id) != logged.end())
        return;
    logged.push_back(id);
    const auto* row  = conf::Config::instance().find<conf::BuffConfig>(id);
    const auto* rule = row ? conf::Config::instance().find<conf::BuffRuleConfig>(row->ruleId) : nullptr;
    if (rule)
        MG_LOG_W("Buff rule not ported: {}", rule->className);
}

// 状态引用由实例记录实际施加次数，防止未激活实例退出导致下溢。
void enterState(World& world, Entity entity, uint64_t serial, mugen::ExtraState state)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    ++instance->m_appliedRefs;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    ++world.ecs().get<BuffPool>(entity).m_ruleCounters[config.ruleId];
    world.ecs().get<sim::ExtraState>(entity).add(state);
}

// 每次退出仅撤销一次实际施加的状态引用。
void exitState(World& world, Entity entity, uint64_t serial, mugen::ExtraState state)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_appliedRefs == 0)
        return;
    --instance->m_appliedRefs;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    --world.ecs().get<BuffPool>(entity).m_ruleCounters[config.ruleId];
    world.ecs().get<sim::ExtraState>(entity).remove(state);
}

// 霸体进入增加独立状态引用。
void armorEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    playEffect(world, entity, serial);
    enterState(world, entity, serial, mugen::ExtraState::SuperArmor);
}
// 霸体退出撤销本次引用。
void armorExit(World& world, Entity entity, uint64_t serial, Fixed)
{
    exitState(world, entity, serial, mugen::ExtraState::SuperArmor);
}
// 无敌进入增加独立状态引用。
void invincibleEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    playEffect(world, entity, serial);
    enterState(world, entity, serial, mugen::ExtraState::Invincible);
}
// 无敌退出撤销本次引用。
void invincibleExit(World& world, Entity entity, uint64_t serial, Fixed)
{
    exitState(world, entity, serial, mugen::ExtraState::Invincible);
}
// 眩晕门闩只允许一次施加，强制中断可能同步触发其他 Buff。
void stunEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered)
        return;
    instance->m_triggered = true;
    playEffect(world, entity, serial);
    enterState(world, entity, serial, mugen::ExtraState::Stun);
    skill::SkillCaster(world, entity).forcedInterrupt();
    world.ecs().get<Displacement>(entity).velocityScale = {};
}
// 从活动速度规则恢复倍率，控制解除后调用。
void recoverMoveSpeed(World& world, Entity entity)
{
    FixedVec3 scale{"1"_fx, "1"_fx, "1"_fx};
    for (const auto& value : world.ecs().get<BuffPool>(entity).m_instances)
    {
        const auto* row  = conf::Config::instance().find<conf::BuffConfig>(value.m_id);
        const auto* rule = row ? conf::Config::instance().find<conf::BuffRuleConfig>(row->ruleId) : nullptr;
        if (!rule || row->paramValue.size() < 2)
            continue;
        const auto id = conf::buffRuleIdFromName(rule->className);
        if (id == conf::BuffRuleId::BuffSpeed || id == conf::BuffRuleId::BuffFall ||
            id == conf::BuffRuleId::BuffGhostWalk || id == conf::BuffRuleId::BuffSpeedStep)
        {
            scale.x += row->paramValue[0] * Fixed::fromInt(value.m_lifecycle.m_repeat);
            scale.z += row->paramValue[1] * Fixed::fromInt(value.m_lifecycle.m_repeat);
        }
    }
    world.ecs().get<Displacement>(entity).velocityScale = scale;
}
// 眩晕退出恢复移动倍率，其他控制状态仍由独立引用约束。
void stunExit(World& world, Entity entity, uint64_t serial, Fixed)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || !instance->m_triggered)
        return;
    instance->m_triggered = false;
    exitState(world, entity, serial, mugen::ExtraState::Stun);
    if (world.ecs().get<sim::ExtraState>(entity).isCanMove())
        recoverMoveSpeed(world, entity);
}

// 受限状态的重复事件只保留一次状态引用和速度归零。
void controlEnter(World& world, Entity entity, uint64_t serial, mugen::ExtraState state, bool interrupt)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered)
        return;
    instance->m_triggered = true;
    playEffect(world, entity, serial);
    enterState(world, entity, serial, state);
    if (interrupt)
        skill::SkillCaster(world, entity).forcedInterrupt();
    world.ecs().get<Displacement>(entity).velocityScale = {};
}
// 控制状态结束后仅在所有限制解除时恢复各速度型 Buff 的倍率。
void controlExit(World& world, Entity entity, uint64_t serial, mugen::ExtraState state)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || !instance->m_triggered)
        return;
    instance->m_triggered = false;
    exitState(world, entity, serial, state);
    if (world.ecs().get<sim::ExtraState>(entity).isCanMove())
        recoverMoveSpeed(world, entity);
}
// 定身限制移动但不打断当前技能。
void duranceEnter(World& w, Entity e, uint64_t s, Fixed)
{
    controlEnter(w, e, s, mugen::ExtraState::Durance, false);
}
void duranceExit(World& w, Entity e, uint64_t s, Fixed)
{
    controlExit(w, e, s, mugen::ExtraState::Durance);
}
// 麻痹限制移动但不打断当前技能。
void palsyEnter(World& w, Entity e, uint64_t s, Fixed)
{
    controlEnter(w, e, s, mugen::ExtraState::Palsy, false);
}
void palsyExit(World& w, Entity e, uint64_t s, Fixed)
{
    controlExit(w, e, s, mugen::ExtraState::Palsy);
}
// 冻结立即打断技能并限制输入和移动。
void frozenEnter(World& w, Entity e, uint64_t s, Fixed)
{
    controlEnter(w, e, s, mugen::ExtraState::Frozen, true);
}
void frozenExit(World& w, Entity e, uint64_t s, Fixed)
{
    controlExit(w, e, s, mugen::ExtraState::Frozen);
}
// 沉默只影响额外状态，不修改速度。
void silenceEnter(World& w, Entity e, uint64_t s, Fixed)
{
    enterState(w, e, s, mugen::ExtraState::Silence);
}
void silenceExit(World& w, Entity e, uint64_t s, Fixed)
{
    exitState(w, e, s, mugen::ExtraState::Silence);
}
// 隐身状态供逻辑查询；显示由表现事件层负责。
void stealthEnter(World& w, Entity e, uint64_t s, Fixed)
{
    enterState(w, e, s, mugen::ExtraState::Stealth);
}
void stealthExit(World& w, Entity e, uint64_t s, Fixed)
{
    exitState(w, e, s, mugen::ExtraState::Stealth);
}

// 速度规则统一按实例开关叠加，避免 Begin 与 Enter 重复叠加。
void speedChange(World& world, Entity entity, uint64_t serial, bool entering, bool hurt, bool dodge)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered == entering)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.size() < 2)
        return;
    instance->m_triggered = entering;
    const Fixed sign      = entering ? "1"_fx : "-1"_fx;
    auto& displacement    = world.ecs().get<Displacement>(entity);
    displacement.velocityScale.x += config.paramValue[0] * sign;
    displacement.velocityScale.z += config.paramValue[1] * sign;
    auto& attributes = world.ecs().get<Attributes>(entity);
    if (hurt && config.paramValue.size() > 2)
        attributes.extended(100) += config.paramValue[2] * sign;
    if (dodge && config.paramValue.size() > 2)
        attributes.extended(109) += config.paramValue[2] * sign;
}
void speedEnter(World& w, Entity e, uint64_t s, Fixed)
{
    speedChange(w, e, s, true, false, false);
}
void speedExit(World& w, Entity e, uint64_t s, Fixed)
{
    speedChange(w, e, s, false, false, false);
}
void fallEnter(World& w, Entity e, uint64_t s, Fixed)
{
    speedChange(w, e, s, true, true, false);
}
void fallExit(World& w, Entity e, uint64_t s, Fixed)
{
    speedChange(w, e, s, false, true, false);
}
void ghostEnter(World& w, Entity e, uint64_t s, Fixed)
{
    speedChange(w, e, s, true, false, true);
}
void ghostExit(World& w, Entity e, uint64_t s, Fixed)
{
    speedChange(w, e, s, false, false, true);
}

// 周期伤害规则在进入时走统一生命修改管线。
void dotEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    dealBuffDamage(world, entity, instance->m_context.m_applicator, getAttackValue(world, entity, *instance));
}
// 锁血按配置比例或绝对值叠加到生命限制。
void hpLockChange(World& world, Entity entity, uint64_t serial, bool entering)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    Fixed value = config.paramValue[0];
    if (value < "1"_fx)
        value *= world.ecs().get<Vitals>(entity).maxHp;
    world.ecs().get<Vitals>(entity).m_hpLock += entering ? value : -value;
}
void hpLockEnter(World& w, Entity e, uint64_t s, Fixed)
{
    hpLockChange(w, e, s, true);
}
void hpLockExit(World& w, Entity e, uint64_t s, Fixed)
{
    hpLockChange(w, e, s, false);
}
// 紫毒在开始和进入事件中补充能量。
void purplePoison(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    Fixed value  = config.paramValue[0];
    auto& vitals = world.ecs().get<Vitals>(entity);
    if (value <= "1"_fx)
        value *= vitals.maxEp;
    vitals.ep = std::min(vitals.maxEp, vitals.ep + value);
}
// 爆发效果只在未处于爆发状态时启动。
void crazyEnter(World& world, Entity entity, uint64_t, Fixed)
{
    auto& pool = world.ecs().get<SkillPool>(entity);
    if (!pool.m_crazy)
        pool.m_crazy = true;
}
// 增减指定施加者的嘲讽值，并保持来源首次出现顺序。
void changeSneer(World& world, Entity entity, Entity source, Fixed delta)
{
    auto& recounts   = world.ecs().get<RoleTimers>(entity).m_sneerRecounts;
    const auto entry = std::find_if(recounts.begin(), recounts.end(),
                                    [source](const SneerRecount& value) { return value.m_source == source; });
    if (entry == recounts.end())
    {
        if (delta > Fixed{})
            recounts.push_back({source, delta});
        return;
    }
    entry->m_value += delta;
    if (entry->m_value <= Fixed{})
        recounts.erase(entry);
}
// 嘲讽进入按配置值向施加者累计权重。
void sneerEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    changeSneer(world, entity, instance->m_context.m_applicator, config.paramValue[0]);
}
// 嘲讽退出对同一施加者撤销本实例权重。
void sneerExit(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    changeSneer(world, entity, instance->m_context.m_applicator, -config.paramValue[0]);
}
// 星盾按层数叠加规避伤害扩展属性。
void starShieldChange(World& world, Entity entity, uint64_t serial, bool entering)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    world.ecs().get<Attributes>(entity).extended(101) += entering ? config.paramValue[0] : -config.paramValue[0];
}
void starShieldEnter(World& w, Entity e, uint64_t s, Fixed)
{
    starShieldChange(w, e, s, true);
}
void starShieldExit(World& w, Entity e, uint64_t s, Fixed)
{
    starShieldChange(w, e, s, false);
}
// 强致盲按层数叠加命中扩展属性。
void superBlindChange(World& world, Entity entity, uint64_t serial, bool entering)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    world.ecs().get<Attributes>(entity).extended(111) += entering ? config.paramValue[0] : -config.paramValue[0];
}
void superBlindEnter(World& w, Entity e, uint64_t s, Fixed)
{
    superBlindChange(w, e, s, true);
}
void superBlindExit(World& w, Entity e, uint64_t s, Fixed)
{
    superBlindChange(w, e, s, false);
}
// 分离效果仅在角色移动时造成一次标准 Buff 伤害。
void separateEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (instance && BuffHolder(world, entity).isMoved())
        dealBuffDamage(world, entity, instance->m_context.m_applicator, getAttackValue(world, entity, *instance));
}
// 白帧只发出表现事件，表现层不得回写模拟状态。
void whiteFrameEnter(World& world, Entity entity, uint64_t, Fixed)
{
    world.emit(entity, WhiteFrame{true});
}
void whiteFrameExit(World& world, Entity entity, uint64_t, Fixed)
{
    world.emit(entity, WhiteFrame{false});
}
// 条件循环按条件结果增减参数中的 Buff，并转发已声明的能力接缝。
void loopCheck(World& world, Entity entity, uint64_t serial, Fixed, bool latched)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const bool passed = checkConditions(world, entity, *instance);
    if (latched && instance->m_triggered == passed)
        return;
    instance->m_triggered = passed;
    const auto& config    = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (!passed && config.bindSpecialAbilityId > 0 && world.hitHooks().m_roleAbilities)
        world.hitHooks().m_roleAbilities(world, entity, true);
    for (const auto value : config.paramValue)
    {
        const int32_t id = value.toInt();
        if (id <= 0)
            continue;
        if (!passed)
            addEntityBuff(world, entity, id);
        else
            BuffHolder(world, entity).removeBuff(id, true);
    }
}
void loopCheckUpdate(World& w, Entity e, uint64_t s, Fixed d)
{
    loopCheck(w, e, s, d, true);
}
void loopCheck2Update(World& w, Entity e, uint64_t s, Fixed d)
{
    loopCheck(w, e, s, d, false);
}
// 数值规则只在实际进入后修改一次，Begin 与 Enter 共用这个门闩。
void changeExtend(World& world, Entity entity, uint64_t serial, int32_t attribute, bool entering)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered == entering)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    instance->m_triggered = entering;
    auto& value = world.ecs().get<Attributes>(entity).extended(attribute);
    value += entering ? config.paramValue.front() : -config.paramValue.front();
}
void avoidCritEnter(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 103, true); }
void avoidCritExit(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 103, false); }
void breakArmorEnter(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 101, true); }
void breakArmorExit(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 101, false); }
void critEnter(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 102, true); }
void critExit(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 102, false); }
void damageCritEnter(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 112, true); }
void damageCritExit(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 112, false); }
void damageReductionEnter(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 101, true); }
void damageReductionExit(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 101, false); }
void dodgeEnter(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 109, true); }
void dodgeExit(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 109, false); }
void hitEnter(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 111, true); }
void hitExit(World& w, Entity e, uint64_t s, Fixed) { changeExtend(w, e, s, 111, false); }

// 资源缩放统一在角色池上保存，避免在多个技能实例间重复累积。
enum class ResourceScale : uint8_t { EpConsume, EpRecovery, TpRecovery };
void changeResourceScale(World& world, Entity entity, uint64_t serial, ResourceScale kind, bool entering)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered == entering)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    instance->m_triggered = entering;
    const Fixed value = entering ? config.paramValue.front() : -config.paramValue.front();
    if (kind == ResourceScale::EpConsume)
        world.ecs().get<SkillPool>(entity).m_epScale += value;
    else if (kind == ResourceScale::EpRecovery)
        world.ecs().get<Vitals>(entity).m_epRecoveryScale += value;
    else
        world.ecs().get<Vitals>(entity).mpRecoveryScale += value;
}
void epConsumeEnter(World& w, Entity e, uint64_t s, Fixed) { changeResourceScale(w, e, s, ResourceScale::EpConsume, true); }
void epConsumeExit(World& w, Entity e, uint64_t s, Fixed) { changeResourceScale(w, e, s, ResourceScale::EpConsume, false); }
void epRecoveryEnter(World& w, Entity e, uint64_t s, Fixed) { changeResourceScale(w, e, s, ResourceScale::EpRecovery, true); }
void epRecoveryExit(World& w, Entity e, uint64_t s, Fixed) { changeResourceScale(w, e, s, ResourceScale::EpRecovery, false); }
void tpRecoveryEnter(World& w, Entity e, uint64_t s, Fixed) { changeResourceScale(w, e, s, ResourceScale::TpRecovery, true); }
void tpRecoveryExit(World& w, Entity e, uint64_t s, Fixed) { changeResourceScale(w, e, s, ResourceScale::TpRecovery, false); }

// 增伤规则按来源的层数或生命差计算，并在退出时精确撤销已加入的值。
void damageHurtChange(World& world, Entity entity, uint64_t serial, bool entering, bool repeat, bool lostHp)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered == entering)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    if (!entering)
    {
        if (!instance->m_triggered)
            return;
        instance->m_triggered = false;
        world.ecs().get<Attributes>(entity).extended(100) -= instance->m_ruleValue;
        instance->m_ruleValue = {};
        return;
    }
    Fixed value = config.paramValue.front();
    if (repeat)
        value *= Fixed::fromInt(instance->m_lifecycle.m_repeat);
    if (lostHp)
    {
        const auto& vitals = world.ecs().get<Vitals>(entity);
        value *= (vitals.maxHp - vitals.hp) / vitals.maxHp;
    }
    instance->m_triggered = true;
    instance->m_ruleValue = value;
    world.ecs().get<Attributes>(entity).extended(100) += value;
}
void damageHurtEnter(World& w, Entity e, uint64_t s, Fixed) { damageHurtChange(w, e, s, true, true, false); }
void damageHurtExit(World& w, Entity e, uint64_t s, Fixed) { damageHurtChange(w, e, s, false, true, false); }
void damageHurt2Enter(World& w, Entity e, uint64_t s, Fixed) { damageHurtChange(w, e, s, true, false, false); }
void damageHurt2Exit(World& w, Entity e, uint64_t s, Fixed) { damageHurtChange(w, e, s, false, false, false); }
void damageHPLossBegin(World& w, Entity e, uint64_t s, Fixed) { damageHurtChange(w, e, s, true, false, true); }
void damageHPLossEnd(World& w, Entity e, uint64_t s, Fixed) { damageHurtChange(w, e, s, false, false, true); }

// 立即资源规则使用绝对值或上限比例，均由统一生命修改入口处理。
void hpEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    const auto& vitals = world.ecs().get<Vitals>(entity);
    Fixed value = config.paramValue.front();
    if (value < "1"_fx && value >= Fixed{})
        value *= vitals.maxHp;
    if (value < Fixed{} && config.paramValue.size() > 1 && config.paramValue[1] > Fixed{})
        value = std::max(value, config.paramValue[1] - vitals.hp);
    Role(world, entity).modifyHp(value, {}, value < Fixed{} && config.paramValue.size() > 1 && config.paramValue[1] > Fixed{});
}
void epEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    auto& vitals = world.ecs().get<Vitals>(entity);
    Fixed value = config.paramValue.front();
    if (value <= "1"_fx)
        value *= vitals.maxEp;
    vitals.ep = std::clamp(vitals.ep + value, Fixed{}, vitals.maxEp);
}
void tpEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    auto& vitals = world.ecs().get<Vitals>(entity);
    Fixed value = config.paramValue.front();
    if (value <= "1"_fx)
        value *= vitals.maxMp;
    vitals.mp = std::clamp(vitals.mp + value, Fixed{}, vitals.maxMp);
}

// 技能冷却和魔法消耗按绑定技能筛选；没有绑定时作用于全部技能。
void changeSkillScale(World& world, Entity entity, uint64_t serial, bool entering, bool cooldown)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered == entering)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    instance->m_triggered = entering;
    const Fixed value = entering ? config.paramValue.front() : -config.paramValue.front();
    auto& skills = world.ecs().get<SkillPool>(entity);
    const auto target = instance->m_context.m_boundSkill;
    for (auto& skill : skills.m_skills)
        if (!target || skill.m_configId == *target)
            (cooldown ? skill.m_coldScale : skill.m_mpScale) += value;
    if (!target && !cooldown)
        skills.m_mpScale += value;
}
void cdSkillEnter(World& w, Entity e, uint64_t s, Fixed) { changeSkillScale(w, e, s, true, true); }
void cdSkillExit(World& w, Entity e, uint64_t s, Fixed) { changeSkillScale(w, e, s, false, true); }
void tpConsumeEnter(World& w, Entity e, uint64_t s, Fixed) { changeSkillScale(w, e, s, true, false); }
void tpConsumeExit(World& w, Entity e, uint64_t s, Fixed) { changeSkillScale(w, e, s, false, false); }

// 护盾进入可附加效果，退出时清零并应用结束效果。
void shieldChange(World& world, Entity entity, uint64_t serial, bool entering)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.empty())
        return;
    auto& vitals = world.ecs().get<Vitals>(entity);
    if (entering)
    {
        Fixed value = config.paramValue[0];
        vitals.m_shield = value > "1"_fx ? value : vitals.maxHp * value;
        if (config.paramValue.size() > 1 && config.paramValue[1].toInt() > 0)
            addEntityBuff(world, entity, config.paramValue[1].toInt());
        auto* refreshed = BuffHolder(world, entity).findInstance(serial);
        if (refreshed && config.paramValue.size() > 3 && config.paramValue[3] > Fixed{})
            refreshed->m_lifecycle.m_repeat = config.paramValue[3].toInt();
    }
    else
    {
        vitals.m_shield = {};
        if (config.paramValue.size() > 1 && config.paramValue[1].toInt() > 0)
            BuffHolder(world, entity).removeBuff(config.paramValue[1].toInt(), true);
        if (config.paramValue.size() > 2 && config.paramValue[2].toInt() > 0)
            addEntityBuff(world, entity, config.paramValue[2].toInt());
    }
}
void shieldEnter(World& w, Entity e, uint64_t s, Fixed) { shieldChange(w, e, s, true); }
void shieldExit(World& w, Entity e, uint64_t s, Fixed) { shieldChange(w, e, s, false); }

// 移除规则先固定候选序号，避免退出钩子改变当前池的遍历位置。
enum class RemoveSelector : uint8_t { Id, Rule, Type };
void removeSelected(World& world, Entity entity, uint64_t serial, RemoveSelector selector, bool partial)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    std::vector<uint64_t> selected;
    for (const auto& candidate : world.ecs().get<BuffPool>(entity).m_instances)
    {
        const auto* row = conf::Config::instance().find<conf::BuffConfig>(candidate.m_id);
        if (!row)
            continue;
        for (size_t index = 0; index < config.paramValue.size(); ++index)
        {
            const int32_t value = config.paramValue[index].toInt();
            const bool matches = selector == RemoveSelector::Id ? candidate.m_id == value :
                selector == RemoveSelector::Rule ? row->ruleId == value : row->buffType == value;
            if (matches)
            {
                if (partial && index < config.paramValue2.size() && !config.paramValue2[index].empty())
                    BuffHolder(world, entity).removeRepeat(candidate.m_serial, false, config.paramValue2[index][0].toInt(), true);
                else
                    selected.push_back(candidate.m_serial);
                break;
            }
        }
    }
    for (const auto value : selected)
    {
        auto* candidate = BuffHolder(world, entity).findInstance(value);
        if (candidate)
            BuffHolder(world, entity).removeBuff(candidate->m_id, true);
    }
    auto* self = BuffHolder(world, entity).findInstance(serial);
    if (self)
        BuffHolder(world, entity).exitInstance(serial);
}
void removeByIdEnter(World& w, Entity e, uint64_t s, Fixed) { removeSelected(w, e, s, RemoveSelector::Id, false); }
void removeById2Enter(World& w, Entity e, uint64_t s, Fixed) { removeSelected(w, e, s, RemoveSelector::Id, true); }
void removeByRuleEnter(World& w, Entity e, uint64_t s, Fixed) { removeSelected(w, e, s, RemoveSelector::Rule, false); }
void removeByTypeEnter(World& w, Entity e, uint64_t s, Fixed) { removeSelected(w, e, s, RemoveSelector::Type, false); }

// 连击经验倍率由受击状态保存，退出后恢复默认倍率。
void comboEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    const auto* external = world.ecs().tryGet<BuffExternalConditions>(entity);
    const size_t index = static_cast<size_t>(external ? external->m_comboEvaluate : 0);
    if (index < config.paramValue.size())
        world.ecs().get<RoleHitState>(entity).m_comboScale = config.paramValue[index];
}
void comboExit(World& world, Entity entity, uint64_t, Fixed)
{
    world.ecs().get<RoleHitState>(entity).m_comboScale = "1"_fx;
}

// 数值怒气和能力池由各自系统拥有；这里使用已声明的能力接缝完成绑定。
void specialAbilityEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.bindSpecialAbilityId > 0 && world.hitHooks().m_buffAbility)
        world.hitHooks().m_buffAbility(world, entity, config.bindSpecialAbilityId, BFEvent::Enter, instance->m_context);
}
// 施加器规则按配置顺序添加或撤销子效果，来源上下文由池操作保存。
void addApplicator(World& world, Entity entity, uint64_t serial, Fixed)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered)
        return;
    instance->m_triggered = true;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    for (const auto value : config.paramValue)
        if (value.toInt() > 0)
            addEntityBuff(world, entity, value.toInt(), instance->m_context.m_boundSkill, instance->m_source);
}
void removeApplicator(World& world, Entity entity, uint64_t serial, Fixed)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || !instance->m_triggered)
        return;
    instance->m_triggered = false;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    for (const auto value : config.paramValue)
        if (value.toInt() > 0)
            BuffHolder(world, entity).removeBuff(value.toInt(), true);
}
void addToApplicator(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_context.m_applicator.value == 0)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    for (const auto value : config.paramValue)
        if (value.toInt() > 0)
            addEntityBuff(world, instance->m_context.m_applicator, value.toInt());
}
// 范围检测只对满足阵营和二维距离限制的角色施加后续效果。
void detectorApply(World& world, Entity entity, uint64_t serial)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.size() < 4)
        return;
    const auto identity = world.ecs().get<RoleIdentity>(entity);
    const auto position = world.ecs().get<Transform>(entity).position;
    const int32_t target = config.paramValue[0].toInt();
    world.ecs().each<RoleIdentity, Transform>([&](Entity candidate, const RoleIdentity& other, const Transform& transform) {
        const bool enemy = (identity.hostileCamp & other.camp) != 0;
        const bool selected = target == 4 ? !enemy : target == 5 ? enemy :
            target == 6 ? other.roleType == static_cast<int32_t>(EntityRoleType::kBoss) :
            target == 7 ? other.roleType == static_cast<int32_t>(EntityRoleType::kMonster) || other.roleType == static_cast<int32_t>(EntityRoleType::kElite) :
            target == 9 ? enemy || candidate != entity : false;
        const Fixed dx = transform.position.x - position.x;
        const Fixed dz = transform.position.z - position.z;
        const Fixed distanceX = dx < Fixed{} ? -dx : dx;
        const Fixed distanceZ = dz < Fixed{} ? -dz : dz;
        if (!selected || distanceX > config.paramValue[1] || distanceZ > config.paramValue[2])
            return;
        for (size_t index = 3; index < config.paramValue.size(); ++index)
            if (config.paramValue[index].toInt() > 0)
                addEntityBuff(world, candidate, config.paramValue[index].toInt());
    });
}
void detectorEnter(World& w, Entity e, uint64_t s, Fixed) { detectorApply(w, e, s); }
void detectorUpdate(World& w, Entity e, uint64_t s, Fixed) { detectorApply(w, e, s); }
// 状态施加器在指定行为持续时间达到阈值时添加后续效果。
void stateBegin(World&, Entity, uint64_t, Fixed) {}
void stateUpdate(World&, Entity, uint64_t, Fixed) {}
void stateExit(World&, Entity, uint64_t, Fixed) {}
// 怒气系统未纳入当前角色组件，保留具体空钩子以避免占位规则。
void angerAdd(World&, Entity, uint64_t, Fixed) {}
// 寒冷即时按标准 Buff 伤害入口结算。
void coldEnter(World& world, Entity entity, uint64_t serial, Fixed)
{
    const auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (instance)
        dealBuffDamage(world, entity, instance->m_context.m_applicator, getAttackValue(world, entity, *instance));
}
// 类型渐进规则依赖完整池筛选，暂以进入时快照写入并在结束时精确撤销。
void damageTypeGradationChange(World& world, Entity entity, uint64_t serial, bool entering)
{
    auto* instance = BuffHolder(world, entity).findInstance(serial);
    if (!instance || instance->m_triggered == entering)
        return;
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(instance->m_id);
    if (config.paramValue.size() < 4)
        return;
    if (!entering)
    {
        instance->m_triggered = false;
        world.ecs().get<Attributes>(entity).extended(100) -= instance->m_ruleValue;
        instance->m_ruleValue = {};
        return;
    }
    int32_t count = 0;
    for (const auto& candidate : world.ecs().get<BuffPool>(entity).m_instances)
    {
        const auto* row = conf::Config::instance().find<conf::BuffConfig>(candidate.m_id);
        if (row && std::find(config.paramValue.begin() + 3, config.paramValue.end(), Fixed::fromInt(row->buffType)) != config.paramValue.end())
            ++count;
    }
    const Fixed value = std::min(Fixed::fromInt(count) * config.paramValue[1], config.paramValue[2]);
    instance->m_triggered = true;
    instance->m_ruleValue = value;
    world.ecs().get<Attributes>(entity).extended(100) += value;
}
void damageTypeGradationBegin(World& w, Entity e, uint64_t s, Fixed) { damageTypeGradationChange(w, e, s, true); }
void damageTypeGradationEnd(World& w, Entity e, uint64_t s, Fixed) { damageTypeGradationChange(w, e, s, false); }
// 未移植是实现状态，绝不是配置允许的额外类名。
constexpr RuleDispatch kUnportedRule{{unported, unported, unported, unported, unported}};
// 在编译期构造完整分派表，只有三个验证规则拥有具体行为。
constexpr auto makeDispatch()
{
    std::array<RuleDispatch, conf::kBuffRuleNames.size()> table{};
    table.fill(kUnportedRule);
    table[static_cast<size_t>(conf::BuffRuleId::BuffSuperArmor)] = {{armorEnter, armorExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffInvincible)] = {
        {invincibleEnter, invincibleExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffStun)]    = {{stunEnter, stunExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDurance)] = {
        {duranceEnter, duranceExit, noHook, duranceEnter, duranceExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffPalsy)]  = {{palsyEnter, palsyExit, noHook, palsyEnter, palsyExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffFrozen)] = {{frozenEnter, frozenExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSilence)] = {{silenceEnter, silenceExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffStealth)] = {{stealthEnter, stealthExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSpeed)] = {{speedEnter, speedExit, noHook, speedEnter, speedExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSpeedStep)] = {
        {speedEnter, speedExit, noHook, speedEnter, speedExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffFall)]      = {{fallEnter, fallExit, noHook, fallEnter, fallExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffGhostWalk)] = {
        {ghostEnter, ghostExit, noHook, ghostEnter, ghostExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffBurn)]     = {{dotEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffBleeding)] = {{dotEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffPoison)]   = {{dotEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffChill)]    = {{dotEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffElectron)] = {{dotEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffRepent)]   = {{dotEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffHPLock)]   = {{hpLockEnter, hpLockExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffPurplePoison)] = {
        {purplePoison, noHook, noHook, purplePoison, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffCrazy)]        = {{crazyEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffAdjudication)] = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDark)]         = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDarkMark)]     = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDarkMark2)]    = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffThunder1)]     = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffThunder2)]     = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDeathMark)]    = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffLightMark)]    = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffStoneSkin)]    = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffUnstable)]     = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffReverse)]      = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffMight)]        = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffCutSakura)]    = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffElectricEye)]  = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSkyProtect)]   = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffImmuneDebuff)] = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffElectron2)]    = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSneer)]        = {{sneerEnter, sneerExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffStarShield)]   = {
        {starShieldEnter, starShieldExit, noHook, starShieldEnter, starShieldExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSuperBlind)] = {
        {superBlindEnter, superBlindExit, noHook, superBlindEnter, superBlindExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSeparate)]   = {{separateEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffLoopCheck)]  = {{noHook, noHook, loopCheckUpdate, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffLoopCheck2)] = {{noHook, noHook, loopCheck2Update, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffWhiteFrame)] = {
        {whiteFrameEnter, whiteFrameExit, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffAddByApplicator)] = {
        {addApplicator, noHook, addApplicator, addApplicator, removeApplicator}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffAddByApplicator3)] = {
        {addApplicator, removeApplicator, addApplicator, addApplicator, removeApplicator}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffAddByApplicator4)] = {
        {noHook, noHook, noHook, addToApplicator, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffAddByDetector)] = {
        {detectorEnter, noHook, detectorUpdate, detectorEnter, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffAddByState)] = {
        {noHook, stateExit, stateUpdate, stateBegin, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffAngerAdd)] = {
        {angerAdd, noHook, noHook, angerAdd, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffAvoidCrit)] = {
        {avoidCritEnter, avoidCritExit, noHook, avoidCritEnter, avoidCritExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffBreakArmor)] = {
        {breakArmorEnter, breakArmorExit, noHook, breakArmorEnter, breakArmorExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffCDSkill)] = {
        {cdSkillEnter, cdSkillExit, noHook, cdSkillEnter, cdSkillExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffCold)] = {{coldEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffComboExp)] = {
        {comboEnter, comboExit, noHook, comboEnter, comboExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffCrit)] = {
        {critEnter, critExit, noHook, critEnter, critExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDamageCrit)] = {
        {damageCritEnter, damageCritExit, noHook, damageCritEnter, damageCritExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDamageHPLoss)] = {
        {noHook, noHook, noHook, damageHPLossBegin, damageHPLossEnd}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDamageHurt)] = {
        {damageHurtEnter, damageHurtExit, noHook, damageHurtEnter, damageHurtExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDamageHurt2)] = {
        {damageHurt2Enter, damageHurt2Exit, noHook, damageHurt2Enter, damageHurt2Exit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDamageReduction)] = {
        {damageReductionEnter, damageReductionExit, noHook, damageReductionEnter, damageReductionExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDamageTypeGradation)] = {
        {noHook, noHook, noHook, damageTypeGradationBegin, damageTypeGradationEnd}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffDodge)] = {
        {dodgeEnter, dodgeExit, noHook, dodgeEnter, dodgeExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffEP)] = {{epEnter, noHook, noHook, epEnter, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffEPConsumeScale)] = {
        {epConsumeEnter, epConsumeExit, noHook, epConsumeEnter, epConsumeExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffEPRecoveryScale)] = {
        {epRecoveryEnter, epRecoveryExit, noHook, epRecoveryEnter, epRecoveryExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffHP)] = {{hpEnter, noHook, hpEnter, hpEnter, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffHit)] = {
        {hitEnter, hitExit, noHook, hitEnter, hitExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffRemoveById)] = {
        {removeByIdEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffRemoveById2)] = {
        {removeById2Enter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffRemoveByRule)] = {
        {removeByRuleEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffRemoveByType)] = {
        {removeByTypeEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffShield)] = {
        {shieldEnter, shieldExit, shieldEnter, shieldEnter, shieldExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSign)] = {{noHook, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffSpecialAbilityApplicator)] = {
        {specialAbilityEnter, noHook, noHook, noHook, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffTP)] = {{tpEnter, noHook, noHook, tpEnter, noHook}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffTPConsumeScale)] = {
        {tpConsumeEnter, tpConsumeExit, noHook, tpConsumeEnter, tpConsumeExit}};
    table[static_cast<size_t>(conf::BuffRuleId::BuffTPRecoveryScale)] = {
        {tpRecoveryEnter, tpRecoveryExit, noHook, tpRecoveryEnter, tpRecoveryExit}};
    return table;
}
// 全进程共享的只读函数指针表。
constexpr auto kDispatch = makeDispatch();
}  // namespace
// 返回最高权重来源；同权重保留最早加入的来源。
Entity BuffHolder::sneerTarget() const
{
    const auto* timers = m_world.ecs().tryGet<RoleTimers>(m_entity);
    if (!timers || timers->m_sneerRecounts.empty())
        return {};
    const auto* best = &timers->m_sneerRecounts.front();
    for (const auto& value : timers->m_sneerRecounts)
        if (value.m_value > best->m_value)
            best = &value;
    return best->m_source;
}
// 移动时间大于零表示角色正在保持移动状态。
bool BuffHolder::isMoved() const
{
    const auto* timers = m_world.ecs().tryGet<RoleTimers>(m_entity);
    return timers && timers->m_movedSeconds > Fixed{};
}
// 枚举只来自经过严格验证的配置名称。
const RuleDispatch& ruleDispatch(conf::BuffRuleId rule)
{
    return kDispatch.at(static_cast<size_t>(rule));
}
// 通过五个钩子均为占位函数判定未移植规则。
bool isUnportedRule(conf::BuffRuleId rule)
{
    const auto& dispatch = ruleDispatch(rule);
    return dispatch.m_hooks == kUnportedRule.m_hooks;
}
}  // namespace mugen::sim::buff
