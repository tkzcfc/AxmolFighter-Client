#include "RoleEffectAssets.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/sim/skill/Skill.h"

#include <set>

namespace mugen::sim
{
namespace
{
// 配置里 0 和负数都表示未配置。
bool validId(int32_t id)
{
    return id > 0;
}

// 按角色技能与特效配置递归收集特效 Spine，访问过的 id 记入集合以避免循环。
class Collector
{
public:
    // 按出生时的技能装配展开该角色会用到的特效。
    void addRole(int32_t roleId, int32_t aiDifficulty)
    {
        if (!validId(roleId) || !m_roles.insert(roleId).second)
            return;
        const auto* role = conf::Config::instance().find<conf::RoleConfig>(roleId);
        if (!role)
            return;

        addEffect(role->deathEffectId);
        addEffect(role->koEffectId);
        for (const int32_t buffId : role->buffIds)
            addBuff(buffId);

        const auto occupation = type_conversions::toCharacterClass(roleId);
        if (occupation != CharacterClass::kUnknown)
            addLoadout(skill::SkillAssembly::playerLoadout(skill::SkillAssembly::offlineKit(occupation)));
        else if (!role->aiId.empty())
        {
            const size_t difficulty =
                aiDifficulty > 0 && static_cast<size_t>(aiDifficulty) <= role->aiId.size()
                    ? static_cast<size_t>(aiDifficulty - 1)
                    : role->aiId.size() - 1;
            addLoadout(skill::SkillAssembly::monsterLoadout(role->aiId[difficulty]));
        }
    }

    // 返回已去重且按 id 升序的 Spine 资源列表。
    std::vector<int32_t> spines() const { return {m_spines.begin(), m_spines.end()}; }

private:
    // 仅收录 res_spine 表中真实存在的资源。
    void addSpine(int32_t resSpineId)
    {
        if (!validId(resSpineId))
            return;
        if (!conf::Config::instance().find<conf::ResSpineConfig>(resSpineId))
            return;
        m_spines.insert(resSpineId);
    }

    // 收录 Buff 自身 Spine 及其绑定的特殊能力。
    void addBuff(int32_t buffId)
    {
        if (!validId(buffId))
            return;
        const auto* buff = conf::Config::instance().find<conf::BuffConfig>(buffId);
        if (!buff)
            return;
        addSpine(buff->spineId);
        addSpecialAbility(buff->bindSpecialAbilityId);
    }

    // 遍历技能装配中的全部技能链入口。
    void addLoadout(const skill::SkillLoadout& loadout)
    {
        for (const auto& group : loadout)
            for (const auto& links : group)
                for (const auto& modes : links)
                    for (const auto& entry : modes)
                        addSkill(entry.m_id);
    }

    // 沿 nextSkill 展开一条技能链上的全部动作。
    void addSkill(int32_t skillId)
    {
        while (validId(skillId) && m_skills.insert(skillId).second)
        {
            const auto* skill = conf::Config::instance().find<conf::SkillAttackConfig>(skillId);
            if (!skill)
                return;
            addActionGroups(skill->actionIds);
            addActionGroups(skill->upActionIds);
            skillId = skill->nextSkill;
        }
    }

    // 展开技能动作二维数组。
    void addActionGroups(const std::vector<std::vector<int32_t>>& groups)
    {
        for (const auto& group : groups)
            for (const int32_t actionId : group)
                addAction(actionId);
    }

    // 收录角色动作生成的特效和附加 Buff。
    void addAction(int32_t actionId)
    {
        if (!validId(actionId))
            return;
        const auto* action = conf::Config::instance().find<conf::ActionConfig>(actionId);
        if (!action)
            return;
        for (const auto& effect : action->effectIds)
        {
            const auto* id = std::get_if<int32_t>(&effect);
            if (id)
                addEffect(*id);
        }
        for (const int32_t buffId : action->buffIds)
            addBuff(buffId);
    }

    // 递归收录特效本体、命中特效、后继特效和子动作。
    void addEffect(int32_t effectId)
    {
        if (!validId(effectId) || !m_effects.insert(effectId).second)
            return;
        const auto* effect = conf::Config::instance().find<conf::EffectConfig>(effectId);
        if (!effect)
            return;

        addSpine(effect->resSpineId);
        addEffect(effect->nextEffectId);
        for (const int32_t hitId : effect->hitEffectIds)
            addEffect(hitId);
        for (const int32_t actionId : effect->actionIds)
            addEffectAction(actionId);
        for (const int32_t buffId : effect->debuffId)
            addBuff(buffId);
        if (const auto* all = std::get_if<int32_t>(&effect->buffAllId))
            addBuff(*all);
        else if (const auto* many = std::get_if<std::vector<int32_t>>(&effect->buffAllId))
            for (const int32_t buffId : *many)
                addBuff(buffId);
        for (const auto& buffValue : effect->buffId)
        {
            if (buffValue != buffValue.floor())
                continue;
            addBuff(static_cast<int32_t>(buffValue.toInt()));
        }
        addSpecialAbility(effect->specialabilityId);
        addSpecialAbility(effect->specialabilityAllId);
    }

    // 收录特殊能力的表现 Spine 及其触发特效。
    void addSpecialAbility(int32_t abilityId)
    {
        if (!validId(abilityId))
            return;
        const auto* ability = conf::Config::instance().find<conf::SpecialAbilityConfig>(abilityId);
        if (!ability)
            return;
        addSpine(ability->spineId);
        for (const int32_t effectId : ability->effectId)
            addEffect(effectId);
    }

    // 收录特效动作的子特效，并展开召唤角色。
    void addEffectAction(int32_t actionId)
    {
        if (!validId(actionId))
            return;
        const auto* action = conf::Config::instance().find<conf::ActionEffectConfig>(actionId);
        if (!action)
            return;
        for (const int32_t childId : action->effectIds)
            addEffect(childId);
        if (validId(action->summonId))
            addRole(action->summonId, 0);
    }

    // 已收集的特效 Spine 资源 id。
    std::set<int32_t> m_spines;
    // 已展开的角色，避免召唤环。
    std::set<int32_t> m_roles;
    // 已展开的技能链节点。
    std::set<int32_t> m_skills;
    // 已展开的特效配置。
    std::set<int32_t> m_effects;
};
}  // namespace

std::vector<int32_t> collectRoleEffectSpineIds(int32_t roleId, int32_t aiDifficulty)
{
    Collector collector;
    collector.addRole(roleId, aiDifficulty);
    return collector.spines();
}

}  // namespace mugen::sim
