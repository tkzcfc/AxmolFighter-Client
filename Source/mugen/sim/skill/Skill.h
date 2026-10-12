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
// 从 AI 配置构建怪物角色的技能装配。
SkillLoadout monsterLoadout(int32_t aiId);
// 从玩家技能列表构建可加载的技能装配。
SkillLoadout playerLoadout(const std::vector<PlayerSkill>& skills);
// 按职业专属技能树构建离线玩家的技能列表。
std::vector<PlayerSkill> offlineKit(CharacterClass occupation);
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
// 按职业生成离线调试面板的技能行。
std::vector<OfflineKitRow> offlineKitRows(CharacterClass occupation);
// 按显式装配创建或替换角色的技能池。
void loadSkill(World& world, Entity entity, const SkillLoadout& loadout);
// 按角色 AI 配置加载默认怪物技能池。
void loadSkill(World& world, Entity entity);
// 按槽位、索引、模式和步骤查找技能池条目。
int32_t getGivenSkill(const SkillPool& pool,
                      std::string_view slot,
                      int32_t index = 1,
                      int32_t mode  = 1,
                      int32_t step  = 1,
                      bool raw      = false);
// 根据当前连招状态解析本次输入可施放的技能。
int32_t getFightSkill(const SkillPool& pool, std::string_view slot);
// 去除模式后缀并取得技能槽位基础名称。
std::string_view baseSlot(const SkillPool& pool, const SkillBase& skill);
// 将控制事件转换为技能输入槽位名称。
std::string eventSlot(ControlEvent event);
// 推进角色全部技能的冷却和释放状态。
void update(World& world, Entity entity);
// 推进角色暴走状态关联的技能逻辑。
void updateCrazy(World& world, Entity entity);
// 检查 AI 条件是否允许选择技能。
bool checkAi(World& world, Entity entity, SkillAi& ai);
// 检查 AI 组合条件是否满足。
bool checkComposition(World& world, Entity entity, const SkillAi& ai);
// 检查技能是否满足内部施放约束。
bool isAllowCastInternal(World& world, Entity entity, int32_t index, bool autoCast = false);
// 检查技能是否可以由当前输入或自动施放触发。
bool isAllowCast(World& world, Entity entity, int32_t index, bool autoCast = false);
// 判断技能是否仍处于冷却阶段。
bool isCooling(const SkillBase& skill);
// 比较两个技能的普通施放优先级。
bool isPriority(const SkillBase& next, const SkillBase& current);
// 比较技能池中两个技能的超级优先级。
bool isSuperPriority(const SkillPool& pool, int32_t next, int32_t current);
// 计算施放技能需要消耗的法力值。
Fixed mpCost(const SkillPool& pool, const SkillBase& skill);
// 计算施放技能需要消耗的能量值。
Fixed epCost(const SkillPool& pool, const SkillBase& skill);
// 预设指定槽位的待施放技能。
bool presetSkill(World& world, Entity entity, std::string_view slot, bool autoCast = false);
// 根据按钮按下或抬起处理技能输入。
bool dealWithButton(World& world, Entity entity, ControlEvent event, bool up = false, bool autoCast = false);
// 分发单个控制事件到技能输入处理。
bool dealButtonEvent(World& world, Entity entity, ControlEvent event, bool up = false, bool autoCast = false);
// 将技能池当前技能切换为指定索引。
void setSkillBase(SkillPool& pool, int32_t index);
// 重置角色技能池的临时施放状态。
void resetSkill(World& world, Entity entity);
// 强制中断角色当前技能施放。
void forcedInterrupt(World& world, Entity entity);
// 推进当前技能到下一基础技能步骤。
bool dealWithNextSkillBase(World& world, Entity entity);
// 更新技能池记录的方向输入。
void dealWithDirection(SkillPool& pool, int32_t index);
// 进入技能施放的开始阶段。
void castBegan(World& world, Entity entity, int32_t index);
// 处理施放管线开始时的状态变化。
void dealWithCastPipeBegan(World& world, Entity entity, int32_t index);
// 处理施放管线结束时的状态变化。
void dealWithCastPipeEnded(World& world, Entity entity, int32_t index);
// 根据配置刷新技能的施放管线字段。
void refreshSkillPipe(SkillBase& skill);
// 增加技能剩余可释放次数。
void addSkillReleaseCount(SkillBase& skill);
// 初始化或重置技能冷却状态。
void dealWithCoolDown(SkillBase& skill);
// 为技能冷却累计逻辑时间。
void addColdTime(SkillBase& skill, Fixed time);
// 判断技能是否允许在松开输入后释放。
bool canUpRelease(const SkillBase& skill);
// 中断角色正在进行的跑步技能。
void interruptRun(World& world, Entity entity);
}  // namespace skill
}  // namespace mugen::sim
