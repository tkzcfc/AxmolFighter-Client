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
// 由输入槽位解析下一个可施放技能下标；次数用尽后跨到下一段，链结束返回 kNoSkill。
int32_t getFightSkill(World& world, Entity entity, std::string_view slot);
// 检查技能是否可施放（受击/挣脱、沉默、槽禁用、冷却、资源、曝气 E、伙伴怒气 P）。
bool isAllowCast(World& world, Entity entity, int32_t index);
// 当前施法槽位名是否为指定槽。
bool isCastingSlot(World& world, Entity entity, std::string_view slot);
// 目标是否正在放闪避（F 槽）。
bool isDodging(World& world, Entity entity);
// 命中成立时把待领取伙伴怒气累加到攻击方。
void grantPartnerRageOnHit(World& world, Entity entity);
// sorder 优先级：任一方为 −1，或 next ≥ current。
bool isPriority(World& world, Entity entity, int32_t next, int32_t current);
// 至尊窗：当前技能普通窗已开且含控制 1，或下一招至尊窗已开且含控制 2。
bool isSuperPriority(World& world, Entity entity, int32_t next, int32_t current);
// 取消判定：普通窗同槽或 isPriority；至尊窗走 isSuperPriority（同槽需控制 2）。
bool canCancel(World& world, Entity entity, int32_t next, int32_t current, bool extra);
// 按钮事件 → 槽位处理；返回是否成功预约施法。
bool dealButton(World& world, Entity entity, ControlEvent event);
// 开始一次施法：写入 SkillCastComponent，同步进入首片段，发出 CastEvent Began。
// deferFirstClip 为 true 时，本帧时间轴不推进该片段（施法请求在时间轴之前）。
void beginCast(World& world, Entity entity, int32_t index, bool deferFirstClip = true);
// 结束当前施法：同步退出当前片段并移除片段 Buff，发出 CastEvent Ended；有缓存下一招则接续。
// deferNext 为 true 时（RoleFsm 在 Timeline 之前切招）新技首片段本帧不推进。
void endCast(World& world, Entity entity, bool deferNext = false);
// 强制中断：退出当前片段、清空施法，并立即销毁本技能 autoRelease=0 的特效。
void forceInterrupt(World& world, Entity entity);
}  // namespace skill::rules
}  // namespace mugen::sim
