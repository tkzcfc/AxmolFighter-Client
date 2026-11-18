#pragma once

#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/conf/GameDef.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mugen::sim
{
class World;

namespace view
{
// 描述玩家携带的基础、强化和暴走技能配置。
struct PlayerSkill
{
    // 指定技能所属的输入槽位。
    EnumSkillSlot slot = EnumSkillSlot::Attack;
    // 保存基础技能配置标识。
    int32_t baseId = -1;
    // 保存强化技能配置标识。
    int32_t rockId = -1;
    // 标识是否使用暴走技能变体。
    bool crazy = false;
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

// 按职业专属技能树构建离线玩家的技能列表。
std::vector<PlayerSkill> offlineKit(CharacterClass occupation);
// 按职业生成离线调试面板的技能行。
std::vector<OfflineKitRow> offlineKitRows(CharacterClass occupation);

// 玩家 HUD 资源只读摘要。
struct PlayerHudView
{
    // 是否读到有效玩家。
    bool valid = false;
    // 角色配置标识。
    int32_t roleId = 0;
    // 角色等级。
    int32_t level = 1;
    // 当前生命。
    Fixed hp;
    // 生命上限。
    Fixed maxHp;
    // 当前魔法。
    Fixed mp;
    // 魔法上限。
    Fixed maxMp;
    // 当前能量。
    Fixed ep;
    // 能量上限。
    Fixed maxEp;
};

// 读取玩家生命、魔法和能量。
PlayerHudView readPlayerHud(const World& world, Entity player);

// 伙伴怒气 HUD 只读摘要。
struct PartnerRageHudView
{
    // 是否持有怒气组件。
    bool hasRage = false;
    // 当前怒气。
    Fixed anger;
    // 冻结剩余毫秒。
    Fixed freezeMs;
    // 冻结上限毫秒。
    Fixed maxFreezeMs;
    // 是否处于变身。
    bool transformed = false;
    // 冻结结束后是否等待回退变身。
    bool recoverPending = false;
};

// 读取伙伴怒气与变身冻结。
PartnerRageHudView readPartnerRageHud(const World& world, Entity player);

// 技能槽冷却只读摘要。
struct SkillCooldownView
{
    // 是否正在推进冷却。
    bool cooling = false;
    // 剩余冷却毫秒。
    Fixed remainMs;
    // 当前冷却周期最大毫秒。
    Fixed maxMs;
};

// 读取技能池槽位的第一条技能冷却。
SkillCooldownView readSlotCooldown(const World& world, Entity player, std::string_view poolSlot);

// 生效中的 Buff 图标。
struct ActiveBuffView
{
    // Buff 配置标识。
    int32_t buffId = 0;
    // 层数。
    int32_t stacks = 1;
};

// 收集未移除的 Buff 实例。
void collectActiveBuffs(const World& world, Entity player, std::vector<ActiveBuffView>& out);

// 目标血条只读摘要。
struct TargetBarView
{
    // 是否为可见存活目标。
    bool valid = false;
    // 角色配置标识。
    int32_t roleId = 0;
    // 角色类型。
    int32_t roleType = 0;
    // 角色等级。
    int32_t level = 1;
    // 当前生命。
    Fixed hp;
    // 生命上限。
    Fixed maxHp;
};

// 读取目标血条。
TargetBarView readTargetBar(const World& world, Entity target);

// 战斗房间与结算只读摘要。
struct BattleRoomView
{
    // 是否启用房间流程。
    bool enabled = false;
    // 是否暂停推进世界。
    bool paused = false;
    // 副本标识。
    int32_t copyId = 0;
    // 房间标识。
    int32_t roomId = 0;
    // 关卡标识。
    int32_t stageId = 0;
    // 战斗经过秒数。
    Fixed gameSeconds;
    // 是否还有下一间。
    bool hasNextRoom = false;
    // 本局最高连击。
    int32_t maxCombo = 0;
    // 本局最高评价。
    int32_t maxEvaluate = 0;
    // 是否请求返回城镇。
    bool returnToTown = false;
    // 当前房间状态。
    mugen::RoomState state = mugen::RoomState::None;
    // 待切换队列是否为空。
    bool pendingEmpty = true;
    // 当前波次。
    int32_t wave = 0;
    // 总波次数。
    int32_t totalWaves = 0;
    // 已用复活次数。
    int32_t usedRevives = 0;
    // 复活上限。
    int32_t maxRevives = 0;
    // 是否等待复活。
    bool reviving = false;
    // 是否胜利。
    bool victory = false;
    // 是否失败。
    bool failure = false;
};

// 读取房间流程与本地玩家连击评价。
BattleRoomView readBattleRoom(const World& world, Entity localPlayer);
}  // namespace view
}  // namespace mugen::sim
