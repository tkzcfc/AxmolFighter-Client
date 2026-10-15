#include "Skill.h"
#include "mugen/conf/ReflectedConfig.h"
#include <algorithm>
#include <string>

namespace mugen::sim::skill
{
SkillLoadout SkillAssembly::monsterLoadout(int32_t aiId)
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

SkillLoadout SkillAssembly::playerLoadout(const std::vector<PlayerSkill>& skills)
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

std::vector<PlayerSkill> SkillAssembly::offlineKit(CharacterClass occupation)
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

namespace
{
// 返回技能槽位枚举对应的调试面板名称。
const char* slotLabel(EnumSkillSlot slot)
{
    switch (slot)
    {
    case EnumSkillSlot::Attack: return "Attack";
    case EnumSkillSlot::Sprint: return "Sprint";
    case EnumSkillSlot::Dodge:  return "Dodge";
    case EnumSkillSlot::Crazy:  return "Crazy";
    case EnumSkillSlot::A:      return "A";
    case EnumSkillSlot::B:      return "B";
    case EnumSkillSlot::C:      return "C";
    case EnumSkillSlot::SA:     return "SA";
    case EnumSkillSlot::SB:     return "SB";
    case EnumSkillSlot::SC:     return "SC";
    case EnumSkillSlot::EA:     return "EA";
    case EnumSkillSlot::EB:     return "EB";
    case EnumSkillSlot::SUA:    return "SUA";
    case EnumSkillSlot::SUB:    return "SUB";
    case EnumSkillSlot::SUC:    return "SUC";
    case EnumSkillSlot::Break:  return "Break";
    case EnumSkillSlot::SAP:    return "SAP";
    case EnumSkillSlot::SBP:    return "SBP";
    case EnumSkillSlot::SCP:    return "SCP";
    case EnumSkillSlot::Anger:  return "Anger";
    default:                    return "None";
    }
}

// 按槽位填入调试面板行的按键、池名与叶子标识。
OfflineKitRow makeRow(EnumSkillSlot slot, int32_t leafId, int32_t skillId, int32_t rockId)
{
    OfflineKitRow row;
    row.slot     = slot;
    row.slotName = slotLabel(slot);
    row.leafId   = leafId;
    row.skillId  = skillId;
    switch (slot)
    {
    case EnumSkillSlot::Attack: row.key = "U"; row.poolSlot = "A"; row.hasControlKey = true; break;
    case EnumSkillSlot::Sprint: row.key = "Y"; row.poolSlot = "Y"; row.hasControlKey = true; break;
    case EnumSkillSlot::Dodge:  row.key = "L"; row.poolSlot = "F"; row.hasControlKey = true; break;
    case EnumSkillSlot::Crazy:  row.key = "K"; row.poolSlot = "E"; row.hasControlKey = true; break;
    case EnumSkillSlot::A:      row.key = "I"; row.poolSlot = "B"; row.hasControlKey = true; break;
    case EnumSkillSlot::B:      row.key = "O"; row.poolSlot = "C"; row.hasControlKey = true; break;
    case EnumSkillSlot::C:      row.key = "J"; row.poolSlot = "D"; row.hasControlKey = true; break;
    case EnumSkillSlot::EA:     row.key = "M"; row.poolSlot = "G"; row.hasControlKey = true; break;
    case EnumSkillSlot::EB:     row.key = "N"; row.poolSlot = "H"; row.hasControlKey = true; break;
    case EnumSkillSlot::SUA:    row.key = "8"; row.poolSlot = "I"; row.hasControlKey = true; break;
    case EnumSkillSlot::SUB:    row.key = "9"; row.poolSlot = "J"; row.hasControlKey = true; break;
    case EnumSkillSlot::SUC:    row.key = "0"; row.poolSlot = "K"; row.hasControlKey = true; break;
    case EnumSkillSlot::Break:  row.key = ";"; row.poolSlot = "L"; row.hasControlKey = true; break;
    case EnumSkillSlot::Anger:  row.key = "T"; row.poolSlot = "P"; row.hasControlKey = true; break;
    case EnumSkillSlot::SAP:    row.poolSlot = "M"; break;
    case EnumSkillSlot::SBP:    row.poolSlot = "N"; break;
    case EnumSkillSlot::SCP:    row.poolSlot = "O"; break;
    case EnumSkillSlot::SA:
    case EnumSkillSlot::SB:
    case EnumSkillSlot::SC:
        if (rockId > 0)
        {
            const auto* rock = conf::Config::instance().find<conf::SkillRockConfig>(rockId);
            if (rock && rock->commandIndex > 0)
                row.poolSlot = std::to_string(rock->commandIndex) + "+";
        }
        break;
    default:
        break;
    }
    return row;
}
}  // namespace

// 按职业树生成离线调试面板行，无树时返回空表。
std::vector<OfflineKitRow> SkillAssembly::offlineKitRows(CharacterClass occupation)
{
    std::vector<OfflineKitRow> result;
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
    if (!tree) return result;
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
        if (skillId <= 0) continue;
        EnumSkillSlot slot;
        if (i < kOrdered.size())
            slot = kOrdered[i];
        else if (overflow < kOverflow.size())
            slot = kOverflow[overflow++];
        else
            continue;
        result.push_back(makeRow(slot, leafId, skillId, -1));
    }
    for (const int32_t leafId : tree->leafBuffId)
    {
        if (leafId <= 0) continue;
        if (overflow >= kOverflow.size()) break;
        const int32_t skillId = leafSkill(leafId);
        if (skillId <= 0) continue;
        result.push_back(makeRow(kOverflow[overflow++], leafId, skillId, -1));
    }
    for (size_t i = 0; i < tree->leafRubId.size() && i < kJoy.size(); ++i)
    {
        const int32_t leafId = tree->leafRubId[i];
        if (leafId <= 0) continue;
        const int32_t skillId = leafSkill(leafId);
        if (skillId <= 0) continue;
        int32_t rockId = -1;
        for (const auto& rock : config.table<conf::SkillRockConfig>())
            if (rock.skillId == skillId)
            {
                rockId = rock.id;
                break;
            }
        result.push_back(makeRow(rockId > 0 ? kJoy[i] : kJoyPassive[i], leafId, skillId, rockId));
    }
    bool usedT = false;
    bool usedH = false;
    bool usedQ = false;
    for (const auto& row : result)
    {
        if (row.key == "T") usedT = true;
        if (row.key == "H") usedH = true;
        if (row.key == "Q") usedQ = true;
    }
    for (auto& row : result)
    {
        if (row.slot != EnumSkillSlot::SAP && row.slot != EnumSkillSlot::SBP && row.slot != EnumSkillSlot::SCP)
            continue;
        if (!usedT)
        {
            row.key           = "T";
            row.hasControlKey = true;
            usedT             = true;
        }
        else if (!usedH)
        {
            row.key           = "H";
            row.hasControlKey = true;
            usedH             = true;
        }
        else if (!usedQ)
        {
            row.key           = "Q";
            row.hasControlKey = true;
            usedQ             = true;
        }
    }
    return result;
}
}
