#pragma once
#include "mugen/sim/World.h"
#include "mugen/conf/GameDef.h"
#include <string>
#include <string_view>

namespace mugen::sim
{
namespace skill::rules
{
// 控制事件 → 技能槽位名称。
std::string eventSlot(ControlEvent event);
// 职业 → 离线技能装配（槽位名 + 链首技能 id），读取 skill_tree / skill_leaf。
std::vector<std::pair<std::string, int32_t>> offlineKit(CharacterClass occupation);
// 为角色构建技能簿（展开 next 链），并挂载 SkillBookComponent / SkillCastComponent。
void buildSkillBook(World& world, Entity entity, CharacterClass occupation);
// 由输入槽位解析下一个可施放技能下标；链结束返回 kNoSkill。
int32_t getFightSkill(World& world, Entity entity, std::string_view slot);
// 检查技能是否可施放（冷却、资源、控制状态）。
bool isAllowCast(World& world, Entity entity, int32_t index);
// 按钮事件 → 槽位处理；返回是否成功预约施法。
bool dealButton(World& world, Entity entity, ControlEvent event);
// 开始一次施法：写入 SkillCastComponent，同步进入首片段，发出 CastEvent Began。
// deferFirstClip 为 true 时，本帧时间轴不推进该片段（施法请求在时间轴之前）。
void beginCast(World& world, Entity entity, int32_t index, bool deferFirstClip = true);
// 结束当前施法：同步退出当前片段并移除片段 Buff，发出 CastEvent Ended；有缓存下一招则接续。
void endCast(World& world, Entity entity);
// 强制中断：退出当前片段、清空施法，并立即销毁本技能 autoRelease=0 的特效。
void forceInterrupt(World& world, Entity entity);
}  // namespace skill::rules
}  // namespace mugen::sim
