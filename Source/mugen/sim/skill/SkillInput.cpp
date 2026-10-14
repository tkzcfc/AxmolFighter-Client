#include "Skill.h"
#include "SkillTriggers.h"
#include "mugen/sim/World.h"
#include "mugen/sim/status/Status.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>

namespace mugen::sim::skill
{
std::string eventSlot(ControlEvent event)
{
    const auto value = static_cast<int32_t>(event);
    /// 控制事件编码连续排列，转换为配置使用的槽位名。
    if (event >= ControlEvent::CTR_ATKA && event <= ControlEvent::CTR_ATKQ)
        return std::string(1, static_cast<char>('A' + value - static_cast<int32_t>(ControlEvent::CTR_ATKA)));
    if (event >= ControlEvent::CTR_JOY1 && event <= ControlEvent::CTR_JOY12)
        return std::to_string(value - static_cast<int32_t>(ControlEvent::CTR_JOY1) + 1) + '+';
    if (event >= ControlEvent::CTR_JOYB && event <= ControlEvent::CTR_JOYD)
        return std::string(1, static_cast<char>('B' + value - static_cast<int32_t>(ControlEvent::CTR_JOYB))) + "_2";
    return {};
}
bool presetSkill(World& world, Entity entity, std::string_view slot, bool autoCast)
{
    auto& pool = world.ecs().get<SkillPool>(entity);
    const auto next = getFightSkill(pool, slot);
    if (next == kNoSkill) return false;
    auto& candidate = pool.m_skills.at(next);
    candidate.m_upTouch = false;
    candidate.m_skillTime = {};
    if (!isAllowCast(world, entity, next, autoCast)) return false;
    if (candidate.m_group == 3 || candidate.m_group == 4) candidate.m_controlAngle = pool.m_controlAngle;
    buff::trigger(world, entity, BFEvent::BeforePrepareSkill, {next});
    if (pool.m_current == kNoSkill)
    {
        setSkillBase(pool, next);
        pool.m_slotChanged = true;
        return true;
    }
    auto& current = pool.m_skills.at(pool.m_current);
    const bool same = slot == baseSlot(pool, current);
    if (pool.m_allowInterrupt && isPriority(candidate, current)) setSkillBase(pool, next);
    else
    {
        if (same && slot == "F") return false;
        const auto currentSlot = baseSlot(pool, current);
        for (auto& other : pool.m_skills)
            if (baseSlot(pool, other) == currentSlot && other.m_address.m_index == current.m_address.m_index &&
                other.m_address.m_mode == current.m_address.m_mode && other.m_address.m_step == current.m_address.m_step)
                other.m_nextSkill = next;
    }
    pool.m_slotChanged = !same;
    return true;
}
bool dealWithButton(World& world, Entity entity, ControlEvent event, bool up, bool autoCast)
{
    auto& pool = world.ecs().get<SkillPool>(entity);
    auto slot = eventSlot(event);
    if (slot.empty()) return false;
    if (up)
    {
        auto next = getFightSkill(pool, slot);
        if (next == kNoSkill) next = pool.m_current;
        if (next != kNoSkill) pool.m_skills[next].m_upTouch = true;
        return false;
    }
    if (slot == "A" && pool.m_canSprint)
    {
        const bool sprint = pool.m_current != kNoSkill && baseSlot(pool, pool.m_skills[pool.m_current]) == "Y" &&
                            !pool.m_skills[pool.m_current].m_finally;
        if (sprint || world.ecs().get<RoleStatus>(entity).isRun()) slot = "Y";
    }
    if (event >= ControlEvent::CTR_JOY1 && event <= ControlEvent::CTR_JOY12)
    {
        const auto joy = getGivenSkill(pool, pool.m_crazy ? slot + '+' : slot);
        if (joy == kNoSkill) slot = "A";
        else
        {
            buff::trigger(world, entity, BFEvent::BeforePrepareSkill, {joy});
            if (!isAllowCast(world, entity, joy)) slot = "A";
        }
    }
    if (slot == "E" && (pool.m_crazy || (pool.m_current != kNoSkill && baseSlot(pool, pool.m_skills[pool.m_current]) == "E"))) return false;
    if (pool.m_crazy && getGivenSkill(pool, slot + '+') != kNoSkill) slot += '+';
    if (!world.ecs().get<RoleStatus>(entity).isAttack()) resetSkill(world, entity);
    return presetSkill(world, entity, slot, autoCast);
}
bool dealButtonEvent(World& world, Entity entity, ControlEvent event, bool up, bool autoCast)
{
    const auto& state = world.ecs().get<RoleStatus>(entity);
    if (state.isDeath() || world.ecs().get<Vitals>(entity).hp <= Fixed{} ||
        !world.ecs().get<ExtraState>(entity).isCanControl() || state.isJostled()) return false;
    if (world.ecs().get<SkillPool>(entity).m_angerRecover) return false;
    return dealWithButton(world, entity, event, up, autoCast) && status::dealWithStatus(world, entity, EntityRoleStatus::Attack);
}
void interruptRun(World& world, Entity entity)
{
    auto* pool = world.ecs().tryGet<SkillPool>(entity);
    if (!pool || pool->m_current == kNoSkill) return;
    auto& current = pool->m_skills[pool->m_current];
    const auto& row = conf::Config::instance().get<conf::SkillAttackConfig>(current.m_configId);
    if (row.sorder == -1 || conf::kRunSorder >= row.sorder ||
        (pool->m_allowInterrupt && std::find(row.sorderControlType.begin(), row.sorderControlType.end(), 1) != row.sorderControlType.end()))
    { current.m_nextRun = true; current.m_nextSkill = kNoSkill; }
}
}
