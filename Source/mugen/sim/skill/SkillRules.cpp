#include "mugen/sim/skill/SkillRules.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/BattleConst.h"
#include <algorithm>
#include <array>

namespace mugen::sim
{
// SkillBookComponent::find 实现：按槽位名 + 步骤 + 链 + 模式查找技能下标。
int32_t SkillBookComponent::find(std::string_view slot, int32_t step, int32_t link, int32_t mode) const
{
    for (size_t i = 0; i < m_skills.size(); ++i)
        if (m_slots[static_cast<size_t>(m_skills[i].m_slot)] == slot && m_skills[i].m_step == step &&
            m_skills[i].m_link == link && m_skills[i].m_mode == mode)
            return static_cast<int32_t>(i);
    return kNoSkill;
}

namespace
{
// 槽位枚举 → 技能池槽位名。
std::string slotName(EnumSkillSlot slot)
{
    switch (slot)
    {
    case EnumSkillSlot::Attack:
        return "A";
    case EnumSkillSlot::Sprint:
        return "Y";
    case EnumSkillSlot::Dodge:
        return "F";
    case EnumSkillSlot::Crazy:
        return "E";
    case EnumSkillSlot::A:
        return "B";
    case EnumSkillSlot::B:
        return "C";
    case EnumSkillSlot::C:
        return "D";
    case EnumSkillSlot::SA:
        return "1+";
    case EnumSkillSlot::SB:
        return "2+";
    case EnumSkillSlot::SC:
        return "3+";
    case EnumSkillSlot::EA:
        return "G";
    case EnumSkillSlot::EB:
        return "H";
    case EnumSkillSlot::SUA:
        return "I";
    case EnumSkillSlot::SUB:
        return "J";
    case EnumSkillSlot::SUC:
        return "K";
    case EnumSkillSlot::Break:
        return "L";
    case EnumSkillSlot::SAP:
        return "M";
    case EnumSkillSlot::SBP:
        return "N";
    case EnumSkillSlot::SCP:
        return "O";
    case EnumSkillSlot::Anger:
        return "P";
    default:
        return "";
    }
}

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

// 槽位枚举 + 搓招配置 → 技能池槽位名。
std::string poolSlotName(EnumSkillSlot slot, int32_t rockId)
{
    if (slot == EnumSkillSlot::SA || slot == EnumSkillSlot::SB || slot == EnumSkillSlot::SC)
    {
        if (rockId > 0)
        {
            const auto* rock = conf::Config::instance().find<conf::SkillRockConfig>(rockId);
            if (rock && rock->commandIndex > 0)
                return std::to_string(rock->commandIndex) + '+';
        }
        return slotName(slot);
    }
    return slotName(slot);
}

// 技能配置是否带指定 sorderControl。
bool hasSorderControl(const conf::battle::SkillDef* def, int32_t value)
{
    if (!def)
        return false;
    return std::find(def->sorderControl.begin(), def->sorderControl.end(), value) != def->sorderControl.end();
}

// 技能所属槽位名。
std::string_view slotOf(const SkillBookComponent& book, const SkillRuntime& skill)
{
    return book.m_slots[static_cast<size_t>(skill.m_slot)];
}

// 技能簿是否含指定槽位名。
bool hasNamedSlot(const SkillBookComponent& book, std::string_view name)
{
    for (const auto& slot : book.m_slots)
        if (slot == name)
            return true;
    return false;
}

// 开始冷却：PVP 取 pvpCd，次数制且已在冷却时只续计时。
void startCooldown(World& world, SkillRuntime& skill, const SkillBookComponent& book)
{
    const auto* def = conf::Config::instance().find<conf::battle::SkillDef>(skill.m_configId);
    Fixed cdScale   = Fixed::fromInt(1);
    for (const auto& mod : book.m_mods)
        if (mod.m_field == static_cast<int32_t>(conf::battle::SkillModField::CdScale))
            cdScale += mod.m_value;
    const int64_t cdMs = def ? (world.setup().m_pvp ? def->pvpCdMs : def->cdMs) : 0;
    skill.m_coldMaxTime = Fixed::fromInt(cdMs) * cdScale;
    skill.m_timing      = true;
    if (!(skill.m_cd > Fixed{} && skill.m_releaseMax > 1))
        skill.m_cd = skill.m_coldMaxTime;
    if (!(skill.m_coldMaxTime > Fixed{} || skill.m_cd > Fixed{}))
        skill.m_timing = false;
}

// 给同槽技能追加冷却。
void addColdTime(SkillRuntime& skill, Fixed time)
{
    time *= conf::kColdTimeSale;
    skill.m_timing = true;
    skill.m_cd += time;
    skill.m_coldMaxTime += time;
}

// 施法开始时记录首段技能的伙伴怒气增量。
void beginPartnerRageCast(World& world, Entity entity, const SkillBookComponent& book, int32_t firstIndex)
{
    auto& cast                      = world.ecs().get<SkillCastComponent>(entity);
    cast.m_partnerRagePendingGain   = {};
    cast.m_partnerRagePendingGainActive = false;
    if (firstIndex < 0 || static_cast<size_t>(firstIndex) >= book.m_skills.size())
        return;
    const int32_t firstId = book.m_skills[static_cast<size_t>(firstIndex)].m_configId;
    if (const auto* skill = conf::Config::instance().find<conf::battle::SkillDef>(firstId))
    {
        cast.m_partnerRagePendingGain       = Fixed::fromInt(std::max(0, skill->costRage));
        cast.m_partnerRagePendingGainActive = cast.m_partnerRagePendingGain > Fixed{};
    }
}

// 怒气技能收束时进入变身。动作文件切换由变身切片负责。
void finishPartnerRageCast(World& world, Entity entity, const SkillBookComponent& book, const SkillRuntime& skill)
{
    auto& cast                      = world.ecs().get<SkillCastComponent>(entity);
    cast.m_partnerRagePendingGain   = {};
    cast.m_partnerRagePendingGainActive = false;
    if (slotOf(book, skill) != "P" || !world.ecs().has<RageStateComponent>(entity))
        return;
    auto& rage = world.ecs().get<RageStateComponent>(entity);
    if (rage.m_transformed || cast.m_partnerRageRecoverPending || rage.m_rage < conf::kPartnerRageMaxAnger)
        return;
    rage.m_rage        = {};
    rage.m_freezeMs    = rage.m_maxFreezeMs;
    rage.m_transformed = true;
    cast.m_partnerRageRecoverPending = rage.m_freezeMs <= Fixed{};
}
}  // namespace

namespace skill::rules
{
// 控制事件 → 槽位名称（ATKA..ATKO → A..O，ATKQ → P，JOY → n+）。
std::string eventSlot(ControlEvent event)
{
    const auto value = static_cast<int32_t>(event);
    if (event >= ControlEvent::CTR_ATKA && event <= ControlEvent::CTR_ATKO)
        return std::string(1, static_cast<char>('A' + value - static_cast<int32_t>(ControlEvent::CTR_ATKA)));
    if (event == ControlEvent::CTR_ATKQ)
        return "P";
    if (event >= ControlEvent::CTR_JOY1 && event <= ControlEvent::CTR_JOY12)
        return std::to_string(value - static_cast<int32_t>(ControlEvent::CTR_JOY1) + 1) + '+';
    if (event >= ControlEvent::CTR_JOYB && event <= ControlEvent::CTR_JOYD)
        return std::string(1, static_cast<char>('B' + value - static_cast<int32_t>(ControlEvent::CTR_JOYB))) + "_2";
    return {};
}

// 职业 → 离线技能装配（读取 skill_tree / skill_leaf，含 overflow 与搓招）。
std::vector<std::pair<std::string, int32_t>> offlineKit(CharacterClass occupation)
{
    std::vector<std::pair<std::string, int32_t>> result;
    const auto profession = static_cast<int32_t>(occupation);
    if (profession <= 0)
        return result;
    const auto* tree = findProfessionTree(profession);
    if (!tree)
        return result;

    const auto push = [&](EnumSkillSlot slot, int32_t skillId, int32_t rockId) {
        if (skillId <= 0)
            return;
        const std::string name = poolSlotName(slot, rockId);
        if (name.empty())
            return;
        result.emplace_back(name, skillId);
    };

    if (isPartnerClass(occupation))
    {
        constexpr std::array<EnumSkillSlot, 9> kPrimary = {
            EnumSkillSlot::Attack, EnumSkillSlot::Sprint, EnumSkillSlot::Dodge, EnumSkillSlot::A, EnumSkillSlot::B,
            EnumSkillSlot::C,      EnumSkillSlot::SA,     EnumSkillSlot::SB,    EnumSkillSlot::SC};
        constexpr std::array<EnumSkillSlot, 2> kBuffSlots = {EnumSkillSlot::EA, EnumSkillSlot::EB};
        for (size_t i = 0; i < tree->leafIds.size() && i < kPrimary.size(); ++i)
            push(kPrimary[i], leafSkillId(tree->leafIds[i]), -1);
        for (size_t i = 0; i < tree->leafBuffId.size() && i < kBuffSlots.size(); ++i)
            push(kBuffSlots[i], leafSkillId(tree->leafBuffId[i]), -1);
        for (size_t i = 0; i < tree->leafRubId.size(); ++i)
        {
            const int32_t slotValue = i < tree->leafRubPlace.size() ? tree->leafRubPlace[i] : 0;
            if (slotValue <= static_cast<int32_t>(EnumSkillSlot::None) ||
                slotValue > static_cast<int32_t>(EnumSkillSlot::Anger))
                continue;
            push(static_cast<EnumSkillSlot>(slotValue), leafSkillId(tree->leafRubId[i]), -1);
        }
        return result;
    }

    constexpr std::array<EnumSkillSlot, 10> kOrdered = {
        EnumSkillSlot::Attack, EnumSkillSlot::Sprint, EnumSkillSlot::Dodge, EnumSkillSlot::Crazy, EnumSkillSlot::A,
        EnumSkillSlot::B,      EnumSkillSlot::C,      EnumSkillSlot::EA,    EnumSkillSlot::EB,    EnumSkillSlot::SUA};
    constexpr std::array<EnumSkillSlot, 4> kOverflow = {EnumSkillSlot::SUB, EnumSkillSlot::SUC, EnumSkillSlot::Break,
                                                        EnumSkillSlot::Anger};
    constexpr std::array<EnumSkillSlot, 3> kJoy        = {EnumSkillSlot::SA, EnumSkillSlot::SB, EnumSkillSlot::SC};
    constexpr std::array<EnumSkillSlot, 3> kJoyPassive = {EnumSkillSlot::SAP, EnumSkillSlot::SBP, EnumSkillSlot::SCP};

    size_t overflow = 0;
    for (size_t i = 0; i < tree->leafIds.size(); ++i)
    {
        const int32_t skillId = leafSkillId(tree->leafIds[i]);
        if (skillId <= 0)
            continue;
        EnumSkillSlot slot = EnumSkillSlot::None;
        if (i < kOrdered.size())
            slot = kOrdered[i];
        else if (overflow < kOverflow.size())
            slot = kOverflow[overflow++];
        else
            continue;
        push(slot, skillId, -1);
    }
    for (const int32_t leafId : tree->leafBuffId)
    {
        if (overflow >= kOverflow.size())
            break;
        const int32_t skillId = leafSkillId(leafId);
        if (skillId <= 0)
            continue;
        push(kOverflow[overflow++], skillId, -1);
    }
    for (size_t i = 0; i < tree->leafRubId.size() && i < kJoy.size(); ++i)
    {
        const int32_t skillId = leafSkillId(tree->leafRubId[i]);
        if (skillId <= 0)
            continue;
        const int32_t rockId = rockIdBySkillId(skillId);
        push(rockId > 0 ? kJoy[i] : kJoyPassive[i], skillId, rockId);
    }
    return result;
}

// 构建角色技能簿并挂载 SkillBookComponent / SkillCastComponent。
void buildSkillBook(World& world, Entity entity, CharacterClass occupation)
{
    SkillBookComponent book;
    for (const auto& [slot, firstId] : offlineKit(occupation))
    {
        int32_t slotIndex = kNoSkill;
        int32_t link      = 1;
        for (size_t i = 0; i < book.m_slots.size(); ++i)
        {
            if (book.m_slots[i] != slot)
                continue;
            slotIndex = static_cast<int32_t>(i);
            for (const auto& skill : book.m_skills)
                if (skill.m_slot == slotIndex)
                    link = std::max(link, skill.m_link + 1);
            break;
        }
        if (slotIndex == kNoSkill)
        {
            slotIndex = static_cast<int32_t>(book.m_slots.size());
            book.m_slots.push_back(slot);
        }
        int32_t id    = firstId;
        int32_t step  = 1;
        int32_t start = static_cast<int32_t>(book.m_skills.size());
        while (id != -1)
        {
            const auto* def = conf::Config::instance().find<conf::battle::SkillDef>(id);
            if (!def)
                break;
            SkillRuntime rt;
            rt.m_slot         = slotIndex;
            rt.m_configId     = id;
            rt.m_step         = step;
            rt.m_link         = link;
            rt.m_mode         = 1;
            rt.m_releaseMax   = def->charges == -1 ? 1 : std::max(1, def->charges);
            rt.m_releaseCount = rt.m_releaseMax;
            rt.m_coldMaxTime  = Fixed::fromInt(def->cdMs);
            rt.m_cd           = {};
            book.m_skills.push_back(rt);
            ++step;
            id = def->next;
        }
        for (size_t i = static_cast<size_t>(start); i < book.m_skills.size(); ++i)
            book.m_skills[i].m_stepCount = step - 1;
        for (auto& skill : book.m_skills)
            if (skill.m_slot == slotIndex)
                skill.m_finally = false;
        if (start < static_cast<int32_t>(book.m_skills.size()))
            book.m_skills.back().m_finally = true;
    }
    world.ecs().add(entity, std::move(book));
    world.ecs().add(entity, SkillCastComponent{});
    world.ecs().add(entity, ActionTimelineComponent{});
}

// 由输入槽位解析下一个可施放技能下标。
int32_t getFightSkill(World& world, Entity entity, std::string_view input)
{
    auto& book = world.ecs().get<SkillBookComponent>(entity);
    auto& cast = world.ecs().get<SkillCastComponent>(entity);
    const auto split           = input.find('_');
    const std::string_view slot = input.substr(0, split);
    const int32_t inputMode     = split == std::string_view::npos ? 1 : 2;
    int32_t link = 1, mode = inputMode, step = 1;
    if (cast.m_skill != kNoSkill)
    {
        const auto& current = book.m_skills[static_cast<size_t>(cast.m_skill)];
        if (slot == book.m_slots[static_cast<size_t>(current.m_slot)])
        {
            link = current.m_link;
            mode = current.m_mode;
            step = current.m_step;
            if (current.m_releaseMax <= 1 || current.m_releaseCount == 0)
            {
                if (step < current.m_stepCount)
                    ++step;
                else
                {
                    ++link;
                    step = 1;
                    mode = inputMode;
                }
            }
        }
    }
    int32_t result = book.find(slot, step, link, mode);
    if (result == kNoSkill && mode == 2 && book.find(slot, 1, link, 2) == kNoSkill)
        result = book.find(slot, step, link, 1);
    return result;
}

// 检查技能是否可施放。晶体消耗首批不做，不拦、不扣。
bool isAllowCast(World& world, Entity entity, int32_t index)
{
    auto& book  = world.ecs().get<SkillBookComponent>(entity);
    auto& skill = book.m_skills[static_cast<size_t>(index)];
    const auto* def   = conf::Config::instance().find<conf::battle::SkillDef>(skill.m_configId);
    const auto slot   = slotOf(book, skill);
    const auto& state = world.ecs().get<RoleStateComponent>(entity);
    if (def && def->type == -1 &&
        ((role::isTobeHit(state.m_state) && slot != "L") || (slot == "L" && !role::isCanBreak(state.m_state))))
        return false;
    if (world.ecs().has<StatusFlagsComponent>(entity))
    {
        const auto& flags = world.ecs().get<StatusFlagsComponent>(entity);
        if (!flags.canControl() || flags.active(mugen::ExtraState::Silence))
            return false;
    }
    if (skill.m_releaseCount == 0 && skill.m_cd > Fixed{})
        return false;
    for (const auto& mod : book.m_mods)
        if (mod.m_field == static_cast<int32_t>(conf::battle::SkillModField::DisableSlot) && mod.m_value > Fixed{} &&
            (mod.m_slot < 0 || mod.m_slot == skill.m_slot))
            return false;
    const auto& health = world.ecs().get<HealthComponent>(entity);
    if (def && (Fixed::fromInt(def->costMp) > health.m_mp || Fixed::fromInt(def->costEp) > health.m_ep))
        return false;
    const auto& cast = world.ecs().get<SkillCastComponent>(entity);
    if (slot == "E" && cast.m_crazy)
        return false;
    if (slot == "P")
        if (const auto* rage = world.ecs().tryGet<RageStateComponent>(entity);
            rage && (rage->m_transformed || cast.m_partnerRageRecoverPending || rage->m_rage < conf::kPartnerRageMaxAnger))
            return false;
    return true;
}

// 当前施法槽位名是否为指定槽。
bool isCastingSlot(World& world, Entity entity, std::string_view slot)
{
    if (!world.ecs().has<SkillCastComponent>(entity) || !world.ecs().has<SkillBookComponent>(entity))
        return false;
    const auto& cast = world.ecs().get<SkillCastComponent>(entity);
    const auto& book = world.ecs().get<SkillBookComponent>(entity);
    if (cast.m_skill == kNoSkill || cast.m_skill < 0 || static_cast<size_t>(cast.m_skill) >= book.m_skills.size())
        return false;
    return slotOf(book, book.m_skills[static_cast<size_t>(cast.m_skill)]) == slot;
}

// 目标是否正在放闪避（F 槽）。
bool isDodging(World& world, Entity entity)
{
    return isCastingSlot(world, entity, "F");
}

// 命中成立时把待领取伙伴怒气累加到攻击方。
void grantPartnerRageOnHit(World& world, Entity entity)
{
    if (!world.ecs().has<SkillCastComponent>(entity) || !world.ecs().has<RageStateComponent>(entity))
        return;
    auto& cast = world.ecs().get<SkillCastComponent>(entity);
    if (!cast.m_partnerRagePendingGainActive)
        return;
    auto& rage = world.ecs().get<RageStateComponent>(entity);
    rage.m_rage = std::clamp(rage.m_rage + cast.m_partnerRagePendingGain, Fixed{}, conf::kPartnerRageMaxAnger);
    cast.m_partnerRagePendingGain       = {};
    cast.m_partnerRagePendingGainActive = false;
}

// sorder 优先级：任一方为 −1，或 next ≥ current。
bool isPriority(World& world, Entity entity, int32_t next, int32_t current)
{
    const auto& book = world.ecs().get<SkillBookComponent>(entity);
    const auto* a = conf::Config::instance().find<conf::battle::SkillDef>(book.m_skills[static_cast<size_t>(next)].m_configId);
    const auto* b =
        conf::Config::instance().find<conf::battle::SkillDef>(book.m_skills[static_cast<size_t>(current)].m_configId);
    const int32_t sa = a ? a->sorder : 0;
    const int32_t sb = b ? b->sorder : 0;
    return sa == -1 || sb == -1 || sa >= sb;
}

// 至尊窗：当前技能普通窗已开且含控制 1，或下一招至尊窗已开且含控制 2。
bool isSuperPriority(World& world, Entity entity, int32_t next, int32_t current)
{
    const auto& book = world.ecs().get<SkillBookComponent>(entity);
    const auto& cast = world.ecs().get<SkillCastComponent>(entity);
    const auto* nextDef =
        conf::Config::instance().find<conf::battle::SkillDef>(book.m_skills[static_cast<size_t>(next)].m_configId);
    const auto* curDef =
        conf::Config::instance().find<conf::battle::SkillDef>(book.m_skills[static_cast<size_t>(current)].m_configId);
    return (cast.m_allowInterrupt && hasSorderControl(curDef, 1)) ||
           (cast.m_allowExtraInterrupt && hasSorderControl(nextDef, 2));
}

// 取消判定：普通窗同槽或 isPriority；至尊窗走 isSuperPriority（同槽需控制 2）。
bool canCancel(World& world, Entity entity, int32_t next, int32_t current, bool extra)
{
    const auto& book = world.ecs().get<SkillBookComponent>(entity);
    const auto& src  = book.m_skills[static_cast<size_t>(current)];
    const auto& dst  = book.m_skills[static_cast<size_t>(next)];
    const bool sameSlot = src.m_slot == dst.m_slot;
    if (!extra)
        return sameSlot || isPriority(world, entity, next, current);
    const auto* def = conf::Config::instance().find<conf::battle::SkillDef>(dst.m_configId);
    if (sameSlot)
        return next != current && hasSorderControl(def, 2);
    return isSuperPriority(world, entity, next, current);
}

// 按钮事件 → 槽位处理；A 键在跑动中改走冲刺槽。
bool dealButton(World& world, Entity entity, ControlEvent event)
{
    auto slot = eventSlot(event);
    if (slot.empty())
        return false;
    auto& cast = world.ecs().get<SkillCastComponent>(entity);
    auto& book = world.ecs().get<SkillBookComponent>(entity);
    const auto& state = world.ecs().get<RoleStateComponent>(entity);
    if (slot == "A")
    {
        const bool sprint = cast.m_skill != kNoSkill &&
                            book.m_slots[static_cast<size_t>(book.m_skills[static_cast<size_t>(cast.m_skill)].m_slot)] ==
                                "Y" &&
                            !book.m_skills[static_cast<size_t>(cast.m_skill)].m_finally;
        if (sprint || state.m_state == RoleStateId::Run)
            slot = "Y";
    }
    if (event >= ControlEvent::CTR_JOY1 && event <= ControlEvent::CTR_JOY12)
    {
        const std::string joySlot = cast.m_crazy ? slot + '+' : slot;
        const int32_t joy         = getFightSkill(world, entity, joySlot);
        if (joy == kNoSkill)
            slot = "A";
        else
        {
            BuffContext prepare;
            prepare.m_target  = entity;
            prepare.m_skillId = book.m_skills[static_cast<size_t>(joy)].m_configId;
            buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforePrepareSkill, prepare);
            if (!isAllowCast(world, entity, joy))
                slot = "A";
        }
    }
    if (slot == "E" &&
        (cast.m_crazy || (cast.m_skill != kNoSkill && slotOf(book, book.m_skills[static_cast<size_t>(cast.m_skill)]) == "E")))
        return false;
    if (cast.m_crazy && hasNamedSlot(book, slot + '+'))
        slot += '+';
    const int32_t next = getFightSkill(world, entity, slot);
    if (next == kNoSkill)
        return false;
    BuffContext prepare;
    prepare.m_target  = entity;
    prepare.m_skillId = book.m_skills[static_cast<size_t>(next)].m_configId;
    buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforePrepareSkill, prepare);
    if (!isAllowCast(world, entity, next))
        return false;
    if (cast.m_skill == kNoSkill)
    {
        beginCast(world, entity, next);
        return true;
    }
    const int32_t current = cast.m_skill;
    const auto& curSkill  = book.m_skills[static_cast<size_t>(current)];
    const bool same       = slot == book.m_slots[static_cast<size_t>(curSkill.m_slot)];
    if (cast.m_allowInterrupt && isPriority(world, entity, next, current))
    {
        cast.m_nextSkill = next;
        cast.m_switch    = true;
    }
    else
    {
        if (same && slot == "F")
            return false;
        cast.m_nextSkill = next;
    }
    return true;
}

// 开始一次施法。deferFirstClip 为 true 时本帧时间轴不推进首片段。
void beginCast(World& world, Entity entity, int32_t index, bool deferFirstClip)
{
    auto& book = world.ecs().get<SkillBookComponent>(entity);
    auto& cast = world.ecs().get<SkillCastComponent>(entity);
    auto& skill = book.m_skills[static_cast<size_t>(index)];
    const auto* def = conf::Config::instance().find<conf::battle::SkillDef>(skill.m_configId);
    timeline::rules::exitClip(world, entity);
    BuffContext castCtx;
    castCtx.m_target  = entity;
    castCtx.m_skillId = skill.m_configId;
    if (skill.m_step == 1)
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforeNextSkill, castCtx);
    startCooldown(world, skill, book);
    skill.m_releaseCount = std::max(0, skill.m_releaseCount - 1);
    if (skill.m_step == 1)
        for (auto& other : book.m_skills)
            if (other.m_slot == skill.m_slot && other.m_link == skill.m_link && other.m_mode != skill.m_mode)
            {
                other.m_releaseCount = std::max(0, other.m_releaseCount - 1);
                startCooldown(world, other, book);
            }
    if (skill.m_link > 1 || skill.m_step > 1)
        for (size_t i = 0; i < book.m_skills.size(); ++i)
        {
            if (static_cast<int32_t>(i) == index)
                continue;
            auto& other = book.m_skills[i];
            if (other.m_slot == skill.m_slot && other.m_link == 1)
                addColdTime(other, skill.m_cd);
        }
    cast.m_skill            = index;
    cast.m_nextSkill        = kNoSkill;
    cast.m_nextRun          = false;
    cast.m_allowInterrupt   = false;
    cast.m_allowExtraInterrupt = false;
    cast.m_switch           = false;
    cast.m_interrupted      = false;
    if (def && !cast.m_crazy && world.ecs().has<HealthComponent>(entity))
    {
        auto& health = world.ecs().get<HealthComponent>(entity);
        Fixed mpFactor = Fixed::fromInt(1);
        for (const auto& mod : book.m_mods)
            if (mod.m_field == static_cast<int32_t>(conf::battle::SkillModField::MpCostScale))
                mpFactor += mod.m_value;
        if (world.ecs().has<StatsComponent>(entity))
            mpFactor *= world.ecs().get<StatsComponent>(entity).finalStat(205);
        const Fixed mpCost = Fixed::fromInt(def->costMp) * mpFactor;
        const Fixed epCost = Fixed::fromInt(def->costEp) *
                             (world.ecs().has<StatsComponent>(entity) ? world.ecs().get<StatsComponent>(entity).finalStat(206)
                                                            : Fixed::fromInt(1));
        if (def->costMp > 0)
        {
            health.m_mp = std::max(Fixed{}, health.m_mp - mpCost);
            castCtx.m_tpSpent = mpCost;
            buff::rules::fire(world, entity, conf::battle::TriggerPoint::UseTp, castCtx);
        }
        if (def->costEp > 0)
        {
            health.m_ep = std::max(Fixed{}, health.m_ep - epCost);
            buff::rules::fire(world, entity, conf::battle::TriggerPoint::UseEp, castCtx);
        }
    }
    const std::string slotName(slotOf(book, skill));
    int32_t first = book.find(slotName, 1, skill.m_link, skill.m_mode);
    if (first == kNoSkill)
        first = index;
    beginPartnerRageCast(world, entity, book, first);
    buff::rules::fire(world, entity, conf::battle::TriggerPoint::SkillColdStart, castCtx);
    buff::rules::fire(world, entity, conf::battle::TriggerPoint::BeforeCastSkill, castCtx);
    // 建立动作时间轴（B2 无摇杆，取第 0 方向列）并同步进入首片段。
    ActionTimelineComponent timeline;
    if (def && !def->sequences.empty())
        timeline.m_clips = def->sequences[0];
    timeline.m_index    = 0;
    timeline.m_finished = false;
    world.ecs().get<ActionTimelineComponent>(entity) = std::move(timeline);
    CastEvent event;
    event.m_entity  = entity;
    event.m_skillId = skill.m_configId;
    event.m_slot    = skill.m_slot;
    event.m_phase   = CastPhase::Began;
    world.ecs().singleton<EventQueue<CastEvent>>().push(event);
    auto& clips = world.ecs().get<ActionTimelineComponent>(entity).m_clips;
    if (clips.empty() || !timeline::rules::enterClip(world, entity, clips.front(), false, deferFirstClip))
        endCast(world, entity);
}

// 结束当前施法：同步退出当前片段；有缓存下一招则同帧接续。
void endCast(World& world, Entity entity, bool deferNext)
{
    auto& book = world.ecs().get<SkillBookComponent>(entity);
    auto& cast = world.ecs().get<SkillCastComponent>(entity);
    if (cast.m_skill == kNoSkill)
        return;
    timeline::rules::exitClip(world, entity);
    const auto& skill = book.m_skills[static_cast<size_t>(cast.m_skill)];
    BuffContext castCtx;
    castCtx.m_target  = entity;
    castCtx.m_skillId = skill.m_configId;
    finishPartnerRageCast(world, entity, book, skill);
    buff::rules::fire(world, entity, conf::battle::TriggerPoint::AfterCastSkill, castCtx);
    if (skill.m_finally || skill.m_step >= skill.m_stepCount)
        buff::rules::fire(world, entity, conf::battle::TriggerPoint::AfterNextSkill, castCtx);
    CastEvent event;
    event.m_entity  = entity;
    event.m_skillId = skill.m_configId;
    event.m_slot    = skill.m_slot;
    event.m_phase   = CastPhase::Ended;
    world.ecs().singleton<EventQueue<CastEvent>>().push(event);
    const int32_t next = cast.m_nextSkill;
    if (next != kNoSkill)
    {
        cast.m_nextSkill = kNoSkill;
        // RoleFsm 在 Timeline 之前切招须 deferNext=true，否则本帧 Timeline 会推进新技首片段。
        beginCast(world, entity, next, deferNext);
    }
    else
    {
        cast.m_skill  = kNoSkill;
        cast.m_switch = false;
        auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
        timeline.m_finished     = true;
        timeline.m_deferAdvance = false;
    }
}

// 强制中断当前施法：退出片段 Buff，并立即销毁 autoRelease=0 的特效。
void forceInterrupt(World& world, Entity entity)
{
    if (!world.ecs().has<SkillBookComponent>(entity) || !world.ecs().has<SkillCastComponent>(entity))
        return;
    auto& book = world.ecs().get<SkillBookComponent>(entity);
    auto& cast = world.ecs().get<SkillCastComponent>(entity);
    if (cast.m_skill == kNoSkill)
        return;
    timeline::rules::exitClip(world, entity);
    displacement::rules::reset(world, entity);
    const auto& skill = book.m_skills[static_cast<size_t>(cast.m_skill)];
    const int32_t skillId = skill.m_configId;
    CastEvent event;
    event.m_entity  = entity;
    event.m_skillId = skillId;
    event.m_slot    = skill.m_slot;
    event.m_phase   = CastPhase::Cancelled;
    world.ecs().singleton<EventQueue<CastEvent>>().push(event);
    cast.m_skill       = kNoSkill;
    cast.m_nextSkill   = kNoSkill;
    cast.m_switch      = false;
    cast.m_interrupted = true;
    auto& timeline = world.ecs().get<ActionTimelineComponent>(entity);
    timeline.m_finished     = true;
    timeline.m_deferAdvance = false;
    world.ecs().each<EffectInfoComponent, LifetimeComponent>([&](Entity effect, const EffectInfoComponent& info, const LifetimeComponent& life) {
        if (info.m_owner == entity && info.m_baseSkillId == skillId && life.m_autoRelease == 0 &&
            !world.ecs().has<DespawningComponent>(effect))
        {
            world.ecs().add(effect, DespawningComponent{});
            world.ecs().singleton<EventQueue<DespawnRequest>>().push({effect, DespawnReason::Interrupted});
        }
    });
}
}  // namespace skill::rules
}  // namespace mugen::sim
