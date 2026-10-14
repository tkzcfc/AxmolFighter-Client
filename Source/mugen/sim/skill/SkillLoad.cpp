#include "SkillCaster.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>
#include <limits>

namespace mugen::sim::skill
{
namespace
{
// 按技能组、槽位和特殊技能数量生成稳定的槽位名称。
std::string slotName(size_t group, size_t slot, size_t specialCount)
{
    if (group == 3 || group == 4)
        return std::to_string(slot) + (group == 3 ? "+" : "++");
    const auto start = group == 5 ? 90 - static_cast<int32_t>(specialCount) : 64;
    std::string result(1, static_cast<char>(start + slot));
    if (group == 2)
        result += '+';
    if (group > 5)
        result += '^' + std::to_string(group);
    return result;
}
}  // namespace

SkillCaster::SkillCaster(World& world, Entity entity) : m_world(world), m_entity(entity) {}

// 展开技能链配置并写入角色的技能池和地址索引。
void SkillCaster::loadSkill(const SkillLoadout& loadout)
{
    SkillPool pool;
    const auto& config    = conf::Config::instance();
    const auto& identity  = m_world.ecs().get<RoleIdentity>(m_entity);
    const auto* role      = config.find<conf::RoleConfig>(identity.roleId);
    const size_t specials = loadout.size() > 4 ? loadout[4].size() : 0;
    for (size_t g = 0; g < loadout.size(); ++g)
        for (size_t s = 0; s < loadout[g].size(); ++s)
        {
            const auto& links = loadout[g][s];
            if (links.empty() || links[0].empty() || links[0][0].m_id == -1)
                continue;
            const size_t first         = pool.m_skills.size();
            const bool hasSlotCapacity = pool.m_slots.size() < std::numeric_limits<uint16_t>::max();
            if (!hasSlotCapacity)
            {
                MG_LOG_E("Skill slot code capacity exceeded");
                MG_ASSERT(hasSlotCapacity && "Skill slot code capacity exceeded");
                return;
            }
            const auto code = static_cast<uint16_t>(pool.m_slots.size());
            for (size_t i = 0; i < links.size(); ++i)
                for (size_t m = 0; m < links[i].size(); ++m)
                {
                    const auto entry   = links[i][m];
                    int32_t id         = entry.m_id;
                    const size_t begin = pool.m_skills.size();
                    std::vector<int32_t> visited;
                    while (id != -1)
                    {
                        const bool noCycle = std::find(visited.begin(), visited.end(), id) == visited.end();
                        if (!noCycle)
                        {
                            MG_LOG_E("Cyclic next_skill configuration");
                            MG_ASSERT(noCycle && "Cyclic next_skill configuration");
                            return;
                        }
                        visited.push_back(id);
                        const auto* row = config.find<conf::SkillAttackConfig>(id);
                        if (!row)
                            break;
                        SkillBase skill;
                        skill.m_address      = {code, static_cast<int32_t>(i + 1), static_cast<int32_t>(m + 1),
                                                static_cast<int32_t>(pool.m_skills.size() - begin + 1)};
                        skill.m_configId     = id;
                        skill.m_group        = static_cast<int32_t>(g + 1);
                        skill.m_slotCount    = static_cast<int32_t>(links.size());
                        skill.m_releaseMax   = row->cdCount == -1 ? 1 : std::max(1, row->cdCount);
                        skill.m_releaseCount = skill.m_releaseMax;
                        skill.m_crystal      = row->crystal;
                        refreshSkillPipe(skill);
                        dealWithCoolDown(skill);
                        skill.m_cd     = {};
                        skill.m_timing = false;
                        if (const auto* ai = config.find<conf::SkillAiConfig>(m == 0 ? entry.m_aiId : -1))
                        {
                            skill.m_ai.m_configId = ai->id;
                            skill.m_ai.m_loadCd   = Fixed::fromInt(ai->loadCd);
                            skill.m_ai.m_useCount = ai->useCount;
                            if (role)
                                skill.m_ai.m_moveDistanceFrame = role->velocity * kLogicDtMs * conf::kWalkRate;
                        }
                        pool.m_index.push_back({skill.m_address, static_cast<int32_t>(pool.m_skills.size())});
                        pool.m_skills.push_back(skill);
                        id = row->nextSkill;
                    }
                    for (size_t n = begin; n < pool.m_skills.size(); ++n)
                        pool.m_skills[n].m_stepCount = static_cast<int32_t>(pool.m_skills.size() - begin);
                    if (pool.m_skills.size() > begin && i + 1 == links.size())
                        pool.m_skills.back().m_finally = true;
                }
            if (pool.m_skills.size() > first)
                pool.m_slots.push_back({slotName(g + 1, s + 1, specials)});
        }
    if (auto* existing = m_world.ecs().tryGet<SkillPool>(m_entity))
        *existing = std::move(pool);
    else
        m_world.ecs().add<SkillPool>(m_entity, std::move(pool));
}
// 从角色 AI 配置取得默认装配并加载技能池。
void SkillCaster::loadSkill()
{
    loadSkill(monsterLoadout(m_world.ecs().get<RoleIdentity>(m_entity).aiId));
}
// 返回技能槽位名中不包含模式后缀的基础部分。
std::string_view baseSlot(const SkillPool& pool, const SkillBase& skill)
{
    const std::string_view name = pool.m_slots.at(skill.m_address.m_slot).m_name;
    return name.substr(0, name.find('^'));
}
// 通过完整技能地址查找索引，并在允许时回退到基础槽位名。
int32_t getGivenSkill(const SkillPool& pool, std::string_view slot, int32_t index, int32_t mode, int32_t step, bool raw)
{
    std::string name(slot);
    if (!raw && pool.m_group > 5)
        name += '^' + std::to_string(pool.m_group);
    auto found =
        std::find_if(pool.m_slots.begin(), pool.m_slots.end(), [&](const auto& s) { return s.m_name == name; });
    if (found == pool.m_slots.end() && !raw)
    {
        name  = std::string(slot.substr(0, slot.find('^')));
        found = std::find_if(pool.m_slots.begin(), pool.m_slots.end(), [&](const auto& s) { return s.m_name == name; });
    }
    if (found == pool.m_slots.end())
        return kNoSkill;
    const SkillAddress address{static_cast<uint16_t>(found - pool.m_slots.begin()), index, mode, step};
    const auto entry =
        std::find_if(pool.m_index.begin(), pool.m_index.end(), [&](const auto& e) { return e.m_address == address; });
    return entry == pool.m_index.end() ? kNoSkill : entry->m_skill;
}
// 结合当前连段状态和输入槽位解析下一个可选技能。
int32_t getFightSkill(const SkillPool& pool, std::string_view input)
{
    const auto split        = input.find('_');
    const auto slot         = input.substr(0, split);
    const int32_t inputMode = split == std::string_view::npos ? (pool.m_change > 2 ? pool.m_change : 1) : 2;
    int32_t index = 1, mode = inputMode, step = 1;
    if (pool.m_current != kNoSkill)
    {
        const auto& current = pool.m_skills.at(pool.m_current);
        if (slot == baseSlot(pool, current))
        {
            index = current.m_address.m_index;
            mode  = current.m_address.m_mode;
            step  = current.m_address.m_step;
            if (current.m_releaseMax <= 1 || current.m_releaseCount == 0)
            {
                if (step < current.m_stepCount)
                    ++step;
                else
                {
                    ++index;
                    step = 1;
                    mode = inputMode;
                }
            }
        }
    }
    auto result = getGivenSkill(pool, slot, index, mode, step);
    if (result == kNoSkill && mode == 2 && getGivenSkill(pool, slot, index, 2) == kNoSkill)
        result = getGivenSkill(pool, slot, index);
    return result;
}
}  // namespace mugen::sim::skill
