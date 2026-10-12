#include "Skill.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>

namespace mugen::sim::skill
{
SkillLoadout monsterLoadout(int32_t aiId)
{
    SkillLoadout result(5);
    const auto* ai = conf::Config::instance().find<conf::AiConfig>(aiId);
    if (!ai) return result;
    const std::array<const std::vector<std::vector<int32_t>>*, 5> groups = {
        &ai->skillIds, &ai->crazySkillIds, &ai->joystickSkillIds, &ai->joystickSkillIds, &ai->otherSkillIds};
    for (size_t g = 0; g < groups.size(); ++g)
    {
        const auto& group = *groups[g];
        result[g].resize(group.size());
        for (size_t s = 0; s < group.size(); ++s)
            for (size_t i = 0; i < group[s].size(); ++i)
            {
                int32_t aiSkill = -1;
                const auto& aiIds = g == 1 ? ai->crazySkillAiIds : ai->skillAiIds;
                if (g < 2 && s < aiIds.size() && i < aiIds[s].size()) aiSkill = aiIds[s][i];
                result[g][s].push_back({{group[s][i], aiSkill}});
            }
    }
    return result;
}

SkillLoadout playerLoadout(const std::vector<PlayerSkill>& skills)
{
    SkillLoadout result(5, SkillGroup(16));
    result[4].resize(3);
    /// 配置槽位到装配组内位置的固定映射，-1 表示摇杆指令槽位。
    constexpr std::array<int32_t, 21> slotMap = {0, 1, 2, 6, 5, 2, 3, 4, -1, -1, -1, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const auto& config = conf::Config::instance();
    for (int category = 0; category < 2; ++category)
    {
        for (size_t slot = 1; slot < slotMap.size(); ++slot)
        {
            if (category == 1 && slot == static_cast<size_t>(EnumSkillSlot::Sprint)) continue;
            bool useCrazy = category == 1 && std::any_of(skills.begin(), skills.end(), [slot](const auto& s) {
                return s.m_crazy && static_cast<size_t>(s.m_slot) == slot;
            });
            for (const auto& entry : skills)
            {
                if (static_cast<size_t>(entry.m_slot) != slot || entry.m_crazy != useCrazy) continue;
                const auto* rock = config.find<conf::SkillRockConfig>(entry.m_rockId);
                const bool joy = slotMap[slot] == -1;
                const int32_t targetSlot = joy ? (rock ? rock->commandIndex : 0) : slotMap[slot];
                if (targetSlot <= 0) continue;
                const size_t group = slot == static_cast<size_t>(EnumSkillSlot::Sprint) ? 4 :
                                     static_cast<size_t>((joy ? 2 : 0) + category);
                auto& targetGroup = result[group];
                if (targetGroup.size() < static_cast<size_t>(targetSlot)) targetGroup.resize(targetSlot);
                SkillModes modes;
                const int32_t baseId = joy && entry.m_baseId == -1 && rock ? rock->skillId : entry.m_baseId;
                if (baseId == -1 && !rock) continue;
                const auto* base = config.find<conf::SkillAttackConfig>(baseId);
                modes.push_back({baseId, base ? base->skillAiId : -1});
                if (rock && !joy) modes.push_back({rock->skillId, -1});
                targetGroup[targetSlot - 1].push_back(modes);
                const size_t link = targetGroup[targetSlot - 1].size() - 1;
                const auto* change = config.find<conf::SkillChangeSystemConfig>(baseId);
                if (!change || !config.find<conf::SkillAttackConfig>(change->skillNewId)) continue;
                if (change->skillChange > 2)
                {
                    auto& targetModes = result[group][targetSlot - 1][link];
                    targetModes.resize(static_cast<size_t>(change->skillChange), modes.front());
                    targetModes.back() = {change->skillNewId, -1};
                }
                if (change->skillGroup > 5)
                {
                    if (result.size() < static_cast<size_t>(change->skillGroup)) result.resize(change->skillGroup, SkillGroup(16));
                    auto& changed = result[change->skillGroup - 1][targetSlot - 1];
                    const auto& normal = result[0][targetSlot - 1];
                    while (changed.size() < normal.size()) changed.push_back(normal[changed.size()]);
                    if (changed.size() <= link) changed.resize(link + 1, modes);
                    changed[link][0] = {change->skillNewId, -1};
                }
            }
        }
    }
    return result;
}

std::vector<PlayerSkill> offlineKit(CharacterClass occupation)
{
    std::vector<PlayerSkill> result;
    const int32_t profession = static_cast<int32_t>(occupation);
    if (profession < 1 || profession > 4) return result;
    const conf::SkillTreeConfig* tree = nullptr;
    for (const auto& row : conf::Config::instance().table<conf::SkillTreeConfig>())
    {
        if (row.professionType != profession) continue;
        if (row.treeType == 1)
        {
            tree = &row;
            break;
        }
        if (!tree) tree = &row;
    }
    if (!tree)
    {
        MG_LOG_W("No profession skill tree for occupation {}", profession);
        return result;
    }
    /// 主动叶子按固定顺序映射到输入槽位，溢出叶子改用扩展槽位序列。
    constexpr std::array<EnumSkillSlot, 10> kOrdered = {
        EnumSkillSlot::Attack, EnumSkillSlot::Sprint, EnumSkillSlot::Dodge, EnumSkillSlot::Crazy,
        EnumSkillSlot::A,      EnumSkillSlot::B,      EnumSkillSlot::C,     EnumSkillSlot::EA,
        EnumSkillSlot::EB,     EnumSkillSlot::SUA};
    constexpr std::array<EnumSkillSlot, 4> kOverflow = {EnumSkillSlot::SUB, EnumSkillSlot::SUC,
                                                        EnumSkillSlot::Break, EnumSkillSlot::Anger};
    constexpr std::array<EnumSkillSlot, 3> kJoy        = {EnumSkillSlot::SA, EnumSkillSlot::SB, EnumSkillSlot::SC};
    constexpr std::array<EnumSkillSlot, 3> kJoyPassive = {EnumSkillSlot::SAP, EnumSkillSlot::SBP,
                                                          EnumSkillSlot::SCP};
    const auto& config = conf::Config::instance();
    auto leafSkill     = [&config](int32_t leafId) -> int32_t {
        const auto* leaf = config.find<conf::SkillLeafConfig>(leafId);
        return leaf ? leaf->skillId : -1;
    };
    size_t overflow = 0;
    for (size_t i = 0; i < tree->leafIds.size(); ++i)
    {
        const int32_t leafId = tree->leafIds[i];
        if (leafId <= 0) continue;
        const int32_t skillId = leafSkill(leafId);
        if (skillId <= 0)
        {
            MG_LOG_W("Missing skill leaf {} for offline kit", leafId);
            continue;
        }
        EnumSkillSlot slot;
        if (i < kOrdered.size())
            slot = kOrdered[i];
        else if (overflow < kOverflow.size())
            slot = kOverflow[overflow++];
        else
            continue;
        result.push_back({slot, skillId, -1, false});
    }
    for (const int32_t leafId : tree->leafBuffId)
    {
        if (leafId <= 0) continue;
        if (overflow >= kOverflow.size()) break;
        const int32_t skillId = leafSkill(leafId);
        if (skillId <= 0)
        {
            MG_LOG_W("Missing skill leaf {} for offline kit", leafId);
            continue;
        }
        result.push_back({kOverflow[overflow++], skillId, -1, false});
    }
    for (size_t i = 0; i < tree->leafRubId.size() && i < kJoy.size(); ++i)
    {
        const int32_t leafId = tree->leafRubId[i];
        if (leafId <= 0) continue;
        const int32_t skillId = leafSkill(leafId);
        if (skillId <= 0)
        {
            MG_LOG_W("Missing skill leaf {} for offline kit", leafId);
            continue;
        }
        int32_t rockId = -1;
        for (const auto& rock : config.table<conf::SkillRockConfig>())
            if (rock.skillId == skillId)
            {
                rockId = rock.id;
                break;
            }
        result.push_back({rockId > 0 ? kJoy[i] : kJoyPassive[i], skillId, rockId, false});
    }
    return result;
}
}
