#pragma once
#include "SkillComponents.h"
#include "mugen/conf/GameDef.h"
#include <string>
#include <string_view>

namespace mugen::sim
{
class World;
namespace skill
{
// 描述技能链中一个技能和其可选 AI 配置引用。
struct SkillEntry
{
    // 指向技能配置行，-1 表示链结束。
    int32_t m_id = -1;
    // 指向该技能首段可选的 AI 配置。
    int32_t m_aiId = -1;
};
// 表示同一索引下可切换的技能模式。
using SkillModes = std::vector<SkillEntry>;
// 表示一个输入槽位的模式链集合。
using SkillLinks = std::vector<SkillModes>;
// 表示一个技能分组的全部槽位链接。
using SkillGroup = std::vector<SkillLinks>;
// 表示加载技能池所需的完整分组配置。
using SkillLoadout = std::vector<SkillGroup>;
// 描述玩家携带的基础、强化和暴走技能配置。
struct PlayerSkill
{
    // 指定技能所属的输入槽位。
    EnumSkillSlot m_slot = EnumSkillSlot::Attack;
    // 保存基础技能配置标识。
    int32_t m_baseId = -1;
    // 保存强化技能配置标识。
    int32_t m_rockId = -1;
    // 标识是否使用暴走技能变体。
    bool m_crazy = false;
};
// 离线调试面板一行技能装配信息。
struct OfflineKitRow
{
    // 键盘按键标签，空表示仅调试 Cast。
    std::string key;
    // 技能槽位枚举名称。
    std::string slotName;
    // 技能槽位枚举。
    EnumSkillSlot slot = EnumSkillSlot::None;
    // 技能树叶子标识。
    int32_t leafId = -1;
    // 技能攻击配置标识。
    int32_t skillId = -1;
    // 技能池槽位字符串。
    std::string poolSlot;
    // 是否已绑定键盘输入。
    bool hasControlKey = false;
};
// 无职业树时调试面板显示的英文提示。
inline constexpr std::string_view kNoProfessionTree = "no profession tree";
// 技能装配与控制槽位名的纯配置构造，不持有战斗状态。
class SkillAssembly
{
public:
    // 从 AI 配置构建怪物角色的技能装配。
    static SkillLoadout monsterLoadout(int32_t aiId);
    // 从玩家技能列表构建可加载的技能装配。
    static SkillLoadout playerLoadout(const std::vector<PlayerSkill>& skills);
    // 按职业专属技能树构建离线玩家的技能列表。
    static std::vector<PlayerSkill> offlineKit(CharacterClass occupation);
    // 按职业生成离线调试面板的技能行。
    static std::vector<OfflineKitRow> offlineKitRows(CharacterClass occupation);
    // 将控制事件转换为技能输入槽位名称。
    static std::string eventSlot(ControlEvent event);
};
// 按槽位、索引、模式和步骤查找技能池条目。
int32_t getGivenSkill(const SkillPool& pool,
                      std::string_view slot,
                      int32_t index = 1,
                      int32_t mode  = 1,
                      int32_t step  = 1,
                      bool raw      = false);
}  // namespace skill
}  // namespace mugen::sim
