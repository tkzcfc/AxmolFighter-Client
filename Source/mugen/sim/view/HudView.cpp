#include "mugen/sim/view/HudView.h"

#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/StdC.h"

#include <array>

namespace mugen::sim::view
{
namespace
{
// 离线装配解析后的一行。
struct OfflineResolvedSkill
{
    // 技能槽位。
    EnumSkillSlot slot = EnumSkillSlot::None;
    // 技能树叶子标识。
    int32_t leafId = -1;
    // 技能配置标识。
    int32_t skillId = -1;
    // 搓招配置标识。
    int32_t rockId = -1;
};

// 伙伴职业走独立装配表。
bool isPartnerClass(CharacterClass occupation)
{
    return occupation == CharacterClass::kPartnerWind || occupation == CharacterClass::kPartnerQueen ||
           occupation == CharacterClass::kPartnerRage;
}

// 按职业取主动技能树（优先 treeType=1）。
const conf::SkillTreeConfig* findProfessionTree(int32_t profession)
{
    const conf::SkillTreeConfig* tree = nullptr;
    for (const auto& row : conf::Config::instance().table<conf::SkillTreeConfig>())
    {
        if (row.professionType != profession)
            continue;
        if (row.treeType == 1)
            return &row;
        if (!tree)
            tree = &row;
    }
    return tree;
}

// 叶子 → 技能 id。
int32_t leafSkillId(int32_t leafId)
{
    const auto* leaf = conf::Config::instance().find<conf::SkillLeafConfig>(leafId);
    return leaf ? leaf->skillId : -1;
}

// 技能 id → 搓招配置 id。
int32_t rockIdBySkillId(int32_t skillId)
{
    for (const auto& rock : conf::Config::instance().table<conf::SkillRockConfig>())
        if (rock.skillId == skillId)
            return rock.id;
    return -1;
}

// 经典职业叶子按固定顺序映射到输入槽位。
std::vector<OfflineResolvedSkill> buildClassicOfflineRows(const conf::SkillTreeConfig& tree)
{
    std::vector<OfflineResolvedSkill> result;
    constexpr std::array<EnumSkillSlot, 10> kOrdered = {
        EnumSkillSlot::Attack, EnumSkillSlot::Sprint, EnumSkillSlot::Dodge, EnumSkillSlot::Crazy, EnumSkillSlot::A,
        EnumSkillSlot::B,      EnumSkillSlot::C,      EnumSkillSlot::EA,    EnumSkillSlot::EB,    EnumSkillSlot::SUA};
    constexpr std::array<EnumSkillSlot, 4> kOverflow   = {EnumSkillSlot::SUB, EnumSkillSlot::SUC, EnumSkillSlot::Break,
                                                          EnumSkillSlot::Anger};
    constexpr std::array<EnumSkillSlot, 3> kJoy        = {EnumSkillSlot::SA, EnumSkillSlot::SB, EnumSkillSlot::SC};
    constexpr std::array<EnumSkillSlot, 3> kJoyPassive = {EnumSkillSlot::SAP, EnumSkillSlot::SBP, EnumSkillSlot::SCP};

    size_t overflow = 0;
    for (size_t i = 0; i < tree.leafIds.size(); ++i)
    {
        const int32_t leafId = tree.leafIds[i];
        if (leafId <= 0)
            continue;
        const int32_t skillId = leafSkillId(leafId);
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
        result.push_back({slot, leafId, skillId, -1});
    }
    for (const int32_t leafId : tree.leafBuffId)
    {
        if (leafId <= 0)
            continue;
        if (overflow >= kOverflow.size())
            break;
        const int32_t skillId = leafSkillId(leafId);
        if (skillId <= 0)
        {
            MG_LOG_W("Missing skill leaf {} for offline kit", leafId);
            continue;
        }
        result.push_back({kOverflow[overflow++], leafId, skillId, -1});
    }
    for (size_t i = 0; i < tree.leafRubId.size() && i < kJoy.size(); ++i)
    {
        const int32_t leafId = tree.leafRubId[i];
        if (leafId <= 0)
            continue;
        const int32_t skillId = leafSkillId(leafId);
        if (skillId <= 0)
        {
            MG_LOG_W("Missing skill leaf {} for offline kit", leafId);
            continue;
        }
        const int32_t rockId = rockIdBySkillId(skillId);
        result.push_back({rockId > 0 ? kJoy[i] : kJoyPassive[i], leafId, skillId, rockId});
    }
    return result;
}

// 伙伴职业遵循独立槽位表。
std::vector<OfflineResolvedSkill> buildPartnerOfflineRows(const conf::SkillTreeConfig& tree)
{
    std::vector<OfflineResolvedSkill> result;
    constexpr std::array<EnumSkillSlot, 9> kPrimary = {
        EnumSkillSlot::Attack, EnumSkillSlot::Sprint, EnumSkillSlot::Dodge, EnumSkillSlot::A, EnumSkillSlot::B,
        EnumSkillSlot::C,      EnumSkillSlot::SA,     EnumSkillSlot::SB,    EnumSkillSlot::SC,
    };
    constexpr std::array<EnumSkillSlot, 2> kBuffSlots = {EnumSkillSlot::EA, EnumSkillSlot::EB};

    for (size_t i = 0; i < tree.leafIds.size() && i < kPrimary.size(); ++i)
    {
        const int32_t leafId = tree.leafIds[i];
        if (leafId <= 0)
            continue;
        const int32_t skillId = leafSkillId(leafId);
        if (skillId <= 0)
        {
            MG_LOG_W("Missing skill leaf {} for offline kit", leafId);
            continue;
        }
        result.push_back({kPrimary[i], leafId, skillId, -1});
    }
    for (size_t i = 0; i < tree.leafBuffId.size() && i < kBuffSlots.size(); ++i)
    {
        const int32_t leafId = tree.leafBuffId[i];
        if (leafId <= 0)
            continue;
        const int32_t skillId = leafSkillId(leafId);
        if (skillId <= 0)
        {
            MG_LOG_W("Missing skill leaf {} for offline kit", leafId);
            continue;
        }
        result.push_back({kBuffSlots[i], leafId, skillId, -1});
    }
    for (size_t i = 0; i < tree.leafRubId.size(); ++i)
    {
        const int32_t leafId = tree.leafRubId[i];
        if (leafId <= 0)
            continue;
        const int32_t slotValue = i < tree.leafRubPlace.size() ? tree.leafRubPlace[i] : 0;
        if (slotValue <= static_cast<int32_t>(EnumSkillSlot::None) ||
            slotValue > static_cast<int32_t>(EnumSkillSlot::Anger))
            continue;
        const int32_t skillId = leafSkillId(leafId);
        if (skillId <= 0)
        {
            MG_LOG_W("Missing skill leaf {} for offline kit", leafId);
            continue;
        }
        result.push_back({static_cast<EnumSkillSlot>(slotValue), leafId, skillId, -1});
    }
    return result;
}

// 按职业树生成离线装配行。
std::vector<OfflineResolvedSkill> buildOfflineRows(CharacterClass occupation)
{
    const int32_t profession = static_cast<int32_t>(occupation);
    if (profession <= 0)
        return {};
    const auto* tree = findProfessionTree(profession);
    if (!tree)
    {
        MG_LOG_W("No profession skill tree for occupation {}", profession);
        return {};
    }
    return isPartnerClass(occupation) ? buildPartnerOfflineRows(*tree) : buildClassicOfflineRows(*tree);
}

// 返回技能槽位枚举对应的调试面板名称。
const char* slotLabel(EnumSkillSlot slot)
{
    switch (slot)
    {
    case EnumSkillSlot::Attack:
        return "Attack";
    case EnumSkillSlot::Sprint:
        return "Sprint";
    case EnumSkillSlot::Dodge:
        return "Dodge";
    case EnumSkillSlot::Crazy:
        return "Crazy";
    case EnumSkillSlot::A:
        return "A";
    case EnumSkillSlot::B:
        return "B";
    case EnumSkillSlot::C:
        return "C";
    case EnumSkillSlot::SA:
        return "SA";
    case EnumSkillSlot::SB:
        return "SB";
    case EnumSkillSlot::SC:
        return "SC";
    case EnumSkillSlot::EA:
        return "EA";
    case EnumSkillSlot::EB:
        return "EB";
    case EnumSkillSlot::SUA:
        return "SUA";
    case EnumSkillSlot::SUB:
        return "SUB";
    case EnumSkillSlot::SUC:
        return "SUC";
    case EnumSkillSlot::Break:
        return "Break";
    case EnumSkillSlot::SAP:
        return "SAP";
    case EnumSkillSlot::SBP:
        return "SBP";
    case EnumSkillSlot::SCP:
        return "SCP";
    case EnumSkillSlot::Anger:
        return "Anger";
    default:
        return "None";
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
    case EnumSkillSlot::Attack:
        row.key           = "U";
        row.poolSlot      = "A";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::Sprint:
        row.key           = "Y";
        row.poolSlot      = "Y";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::Dodge:
        row.key           = "L";
        row.poolSlot      = "F";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::Crazy:
        row.key           = "K";
        row.poolSlot      = "E";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::A:
        row.key           = "I";
        row.poolSlot      = "B";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::B:
        row.key           = "O";
        row.poolSlot      = "C";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::C:
        row.key           = "J";
        row.poolSlot      = "D";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::EA:
        row.key           = "M";
        row.poolSlot      = "G";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::EB:
        row.key           = "N";
        row.poolSlot      = "H";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::SUA:
        row.key           = "8";
        row.poolSlot      = "I";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::SUB:
        row.key           = "9";
        row.poolSlot      = "J";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::SUC:
        row.key           = "0";
        row.poolSlot      = "K";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::Break:
        row.key           = ";";
        row.poolSlot      = "L";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::Anger:
        row.key           = "T";
        row.poolSlot      = "P";
        row.hasControlKey = true;
        break;
    case EnumSkillSlot::SAP:
        row.poolSlot = "M";
        break;
    case EnumSkillSlot::SBP:
        row.poolSlot = "N";
        break;
    case EnumSkillSlot::SCP:
        row.poolSlot = "O";
        break;
    case EnumSkillSlot::SA:
    case EnumSkillSlot::SB:
    case EnumSkillSlot::SC:
        if (rockId > 0)
        {
            const auto* rock = conf::Config::instance().find<conf::SkillRockConfig>(rockId);
            if (rock && rock->commandIndex > 0)
                row.poolSlot = std::to_string(rock->commandIndex) + "+";
        }
        else
            row.poolSlot = slot == EnumSkillSlot::SA ? "1+" : slot == EnumSkillSlot::SB ? "2+" : "3+";
        break;
    default:
        break;
    }
    return row;
}

// 待回收或休眠的实体对 HUD 不可见。
bool hidden(const WorldECS& ecs, Entity entity)
{
    return ecs.has<DespawningComponent>(entity) || ecs.has<InactiveComponent>(entity);
}

// 房间是否还有可进入的下一间。
bool roomHasNextRoom(const conf::RoomConfig& room)
{
    const size_t count = room.portalIds.size();
    for (size_t i = 0; i < count; ++i)
    {
        if (room.portalIds[i] <= 0 || i >= room.portalDestRoomIds.size())
            continue;
        for (const int32_t dest : room.portalDestRoomIds[i])
            if (dest > 0)
                return true;
    }
    return false;
}
}  // namespace

// 按职业专属技能树构建离线玩家的技能列表。
std::vector<PlayerSkill> offlineKit(CharacterClass occupation)
{
    std::vector<PlayerSkill> result;
    for (const auto& row : buildOfflineRows(occupation))
        result.push_back({row.slot, row.skillId, row.rockId, false});
    return result;
}

// 按职业生成离线调试面板的技能行。
std::vector<OfflineKitRow> offlineKitRows(CharacterClass occupation)
{
    std::vector<OfflineKitRow> result;
    for (const auto& row : buildOfflineRows(occupation))
        result.push_back(makeRow(row.slot, row.leafId, row.skillId, row.rockId));
    bool usedT = false;
    bool usedH = false;
    bool usedQ = false;
    for (const auto& row : result)
    {
        if (row.key == "T")
            usedT = true;
        if (row.key == "H")
            usedH = true;
        if (row.key == "Q")
            usedQ = true;
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

// 读取玩家生命、魔法和能量。
PlayerHudView readPlayerHud(const World& world, Entity player)
{
    PlayerHudView view;
    const auto& ecs = world.ecs();
    if (!ecs.alive(player) || hidden(ecs, player))
        return view;
    const auto* identity = ecs.tryGet<IdentityComponent>(player);
    const auto* health   = ecs.tryGet<HealthComponent>(player);
    if (!identity || !health)
        return view;
    view.valid  = true;
    view.roleId = identity->configId;
    view.level  = identity->level;
    view.hp     = health->hp;
    view.maxHp  = health->maxHp;
    view.mp     = health->mp;
    view.maxMp  = health->maxMp;
    view.ep     = health->ep;
    view.maxEp  = health->maxEp;
    return view;
}

// 读取伙伴怒气与变身冻结。
PartnerRageHudView readPartnerRageHud(const World& world, Entity player)
{
    PartnerRageHudView view;
    const auto& ecs = world.ecs();
    if (!ecs.alive(player) || hidden(ecs, player))
        return view;
    const auto* rage = ecs.tryGet<RageStateComponent>(player);
    if (!rage)
        return view;
    view.hasRage     = true;
    view.anger       = rage->rage;
    view.freezeMs    = rage->freezeMs;
    view.maxFreezeMs = rage->maxFreezeMs;
    view.transformed = rage->transformed;
    if (const auto* cast = ecs.tryGet<SkillCastComponent>(player))
        view.recoverPending = cast->partnerRageRecoverPending;
    return view;
}

// 读取技能池槽位的第一条技能冷却。
SkillCooldownView readSlotCooldown(const World& world, Entity player, std::string_view poolSlot)
{
    SkillCooldownView view;
    const auto& ecs = world.ecs();
    if (!ecs.alive(player) || hidden(ecs, player))
        return view;
    const auto* book = ecs.tryGet<SkillBookComponent>(player);
    if (!book)
        return view;
    int32_t slotIndex = -1;
    for (size_t i = 0; i < book->slots.size(); ++i)
    {
        if (book->slots[i] != poolSlot)
            continue;
        slotIndex = static_cast<int32_t>(i);
        break;
    }
    if (slotIndex < 0)
        return view;
    for (const auto& skill : book->skills)
    {
        if (skill.slot != slotIndex)
            continue;
        view.cooling  = skill.timing;
        view.remainMs = skill.cd;
        view.maxMs    = skill.coldMaxTime;
        return view;
    }
    return view;
}

// 收集未移除的 Buff 实例。
void collectActiveBuffs(const World& world, Entity player, std::vector<ActiveBuffView>& out)
{
    out.clear();
    const auto& ecs = world.ecs();
    if (!ecs.alive(player) || hidden(ecs, player))
        return;
    const auto* list = ecs.tryGet<BuffListComponent>(player);
    if (!list)
        return;
    for (const auto& inst : list->instances)
    {
        if (inst.removing)
            continue;
        out.push_back({inst.id, inst.stacks});
    }
}

// 读取目标血条。
TargetBarView readTargetBar(const World& world, Entity target)
{
    TargetBarView view;
    const auto& ecs = world.ecs();
    if (!ecs.alive(target) || hidden(ecs, target))
        return view;
    const auto* identity = ecs.tryGet<IdentityComponent>(target);
    const auto* health   = ecs.tryGet<HealthComponent>(target);
    if (!identity || !health || health->isDead())
        return view;
    view.valid    = true;
    view.roleId   = identity->configId;
    view.roleType = identity->roleType;
    view.level    = identity->level;
    view.hp       = health->hp;
    view.maxHp    = health->maxHp;
    return view;
}

// 读取房间流程与本地玩家连击评价。
BattleRoomView readBattleRoom(const World& world, Entity localPlayer)
{
    BattleRoomView view;
    const auto& room = world.ecs().singleton<RoomState>();
    view.enabled      = room.enabled;
    view.paused       = room.paused;
    view.copyId       = room.copyId;
    view.roomId       = room.roomId;
    view.stageId      = room.stageId;
    view.gameSeconds  = room.gameSeconds;
    view.hasNextRoom  = roomHasNextRoom(room.config);
    view.returnToTown = room.returnToTown;
    view.state        = room.state;
    view.pendingEmpty = room.pending.empty();
    view.wave         = room.wave;
    view.totalWaves   = room.config.battleRuleType == 2
                            ? static_cast<int32_t>(room.config.battleRuleParam.size())
                            : (room.config.monsterIds.empty() ? 0 : 1);
    view.usedRevives  = room.usedRevives;
    view.maxRevives   = room.maxRevives;
    view.reviving     = room.reviving;
    view.victory      = room.victory;
    view.failure      = room.failure;
    if (world.ecs().alive(localPlayer))
        if (const auto* combo = world.ecs().tryGet<ComboMeterComponent>(localPlayer))
        {
            view.maxCombo    = combo->maxCount;
            view.maxEvaluate = combo->maxGrade;
        }
    return view;
}
}  // namespace mugen::sim::view
