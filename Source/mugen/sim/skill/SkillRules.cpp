#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include <algorithm>

namespace mugen::sim
{
// SkillBook::find 实现：按槽位名 + 步骤查找技能下标。
int32_t SkillBook::find(std::string_view slot, int32_t step) const
{
    for (size_t i = 0; i < m_skills.size(); ++i)
        if (m_slots[static_cast<size_t>(m_skills[i].m_slot)] == slot && m_skills[i].m_step == step)
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
    case EnumSkillSlot::Anger:
        return "P";
    default:
        return "";
    }
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
    return {};
}

// 职业 → 离线技能装配（读取 skill_tree / skill_leaf）。
std::vector<std::pair<std::string, int32_t>> offlineKit(CharacterClass occupation)
{
    std::vector<std::pair<std::string, int32_t>> result;
    const auto profession = static_cast<int32_t>(occupation);
    if (profession <= 0)
        return result;
    const auto& config = conf::Config::instance();
    const conf::SkillTreeConfig* tree = nullptr;
    for (const auto& row : config.table<conf::SkillTreeConfig>())
    {
        if (row.professionType != profession)
            continue;
        if (row.treeType == 1)
        {
            tree = &row;
            break;
        }
        if (!tree)
            tree = &row;
    }
    if (!tree)
        return result;
    // 主动叶子按固定顺序映射到输入槽位（与旧实现 kOrdered 一致）。
    constexpr std::array<EnumSkillSlot, 10> kOrdered = {
        EnumSkillSlot::Attack, EnumSkillSlot::Sprint, EnumSkillSlot::Dodge, EnumSkillSlot::Crazy, EnumSkillSlot::A,
        EnumSkillSlot::B,      EnumSkillSlot::C,      EnumSkillSlot::EA,    EnumSkillSlot::EB,    EnumSkillSlot::SUA};
    for (size_t i = 0; i < tree->leafIds.size(); ++i)
    {
        const int32_t leafId = tree->leafIds[i];
        if (leafId <= 0)
            continue;
        const auto* leaf = config.find<conf::SkillLeafConfig>(leafId);
        if (!leaf || leaf->skillId <= 0)
            continue;
        const EnumSkillSlot slot = i < kOrdered.size() ? kOrdered[i] : EnumSkillSlot::None;
        const std::string name  = slotName(slot);
        if (name.empty())
            continue;
        result.emplace_back(name, leaf->skillId);
    }
    return result;
}

// 构建角色技能簿并挂载 SkillBook / SkillCast。
void buildSkillBook(World& world, Entity entity, CharacterClass occupation)
{
    SkillBook book;
    for (const auto& [slot, firstId] : offlineKit(occupation))
    {
        const int32_t slotIndex = static_cast<int32_t>(book.m_slots.size());
        book.m_slots.push_back(slot);
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
        if (!book.m_skills.empty())
            book.m_skills.back().m_finally = true;
    }
    world.ecs().add(entity, std::move(book));
    world.ecs().add(entity, SkillCast{});
    world.ecs().add(entity, ActionTimeline{});
}

// 由输入槽位解析下一个可施放技能下标。
int32_t getFightSkill(World& world, Entity entity, std::string_view slot)
{
    auto& book = world.ecs().get<SkillBook>(entity);
    auto& cast = world.ecs().get<SkillCast>(entity);
    int32_t step = 1;
    if (cast.m_skill != kNoSkill)
    {
        const auto& current = book.m_skills[static_cast<size_t>(cast.m_skill)];
        if (slot == book.m_slots[static_cast<size_t>(current.m_slot)])
        {
            step = current.m_step;
            if (current.m_releaseMax <= 1 || current.m_releaseCount == 0)
            {
                if (step < current.m_stepCount)
                    ++step;
                else
                    return kNoSkill;  // 链已结束（B2 无跨连段续接）
            }
        }
    }
    return book.find(slot, step);
}

// 检查技能是否可施放。
bool isAllowCast(World& world, Entity entity, int32_t index)
{
    auto& book  = world.ecs().get<SkillBook>(entity);
    auto& skill = book.m_skills[static_cast<size_t>(index)];
    if (skill.m_releaseCount == 0 && skill.m_cd > Fixed{})
        return false;
    const auto& health = world.ecs().get<Health>(entity);
    const auto* def    = conf::Config::instance().find<conf::battle::SkillDef>(skill.m_configId);
    if (def && (def->costMp > 0 || def->costEp > 0 || def->costCrystal > 0))
    {
        if (Fixed::fromInt(def->costMp) > health.m_mp || Fixed::fromInt(def->costEp) > health.m_ep)
            return false;
    }
    return true;
}

// 按钮事件 → 槽位处理；A 键在跑动中改走冲刺槽。
bool dealButton(World& world, Entity entity, ControlEvent event)
{
    auto slot = eventSlot(event);
    if (slot.empty())
        return false;
    auto& cast = world.ecs().get<SkillCast>(entity);
    auto& book = world.ecs().get<SkillBook>(entity);
    const auto& state = world.ecs().get<RoleState>(entity);
    if (slot == "A")
    {
        const bool sprint = cast.m_skill != kNoSkill &&
                            book.m_slots[static_cast<size_t>(book.m_skills[static_cast<size_t>(cast.m_skill)].m_slot)] ==
                                "Y" &&
                            !book.m_skills[static_cast<size_t>(cast.m_skill)].m_finally;
        if (sprint || state.m_state == RoleStateId::Run)
            slot = "Y";
    }
    const int32_t next = getFightSkill(world, entity, slot);
    if (next == kNoSkill)
        return false;
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
    if (cast.m_allowInterrupt)
        beginCast(world, entity, next);
    else if (same)
        cast.m_nextSkill = next;
    return true;
}

// 开始一次施法。
void beginCast(World& world, Entity entity, int32_t index)
{
    auto& book = world.ecs().get<SkillBook>(entity);
    auto& cast = world.ecs().get<SkillCast>(entity);
    auto& skill = book.m_skills[static_cast<size_t>(index)];
    const auto* def = conf::Config::instance().find<conf::battle::SkillDef>(skill.m_configId);
    skill.m_coldMaxTime = Fixed::fromInt(def ? def->cdMs : 0);
    skill.m_cd          = skill.m_coldMaxTime;
    skill.m_timing      = skill.m_coldMaxTime > Fixed{};
    skill.m_releaseCount = std::max(0, skill.m_releaseCount - 1);
    cast.m_skill            = index;
    cast.m_nextSkill        = kNoSkill;
    cast.m_nextRun          = false;
    cast.m_allowInterrupt   = false;
    cast.m_allowExtraInterrupt = false;
    cast.m_interrupted      = false;
    // 建立动作时间轴（B2 无摇杆，取第 0 方向列）。
    ActionTimeline timeline;
    if (def && !def->sequences.empty())
        timeline.m_clips = def->sequences[0];
    timeline.m_index    = 0;
    timeline.m_finished = false;
    world.ecs().get<ActionTimeline>(entity) = std::move(timeline);
    CastEvent event;
    event.m_entity  = entity;
    event.m_skillId = skill.m_configId;
    event.m_slot    = book.m_slots[static_cast<size_t>(skill.m_slot)];
    event.m_phase   = CastPhase::Began;
    world.ecs().singleton<EventQueue<CastEvent>>().push(event);
}

// 结束当前施法；有缓存下一招则接续。
void endCast(World& world, Entity entity)
{
    auto& book = world.ecs().get<SkillBook>(entity);
    auto& cast = world.ecs().get<SkillCast>(entity);
    if (cast.m_skill == kNoSkill)
        return;
    const auto& skill = book.m_skills[static_cast<size_t>(cast.m_skill)];
    CastEvent event;
    event.m_entity  = entity;
    event.m_skillId = skill.m_configId;
    event.m_slot    = book.m_slots[static_cast<size_t>(skill.m_slot)];
    event.m_phase   = CastPhase::Ended;
    world.ecs().singleton<EventQueue<CastEvent>>().push(event);
    const int32_t next = cast.m_nextSkill;
    if (next != kNoSkill)
    {
        cast.m_nextSkill = kNoSkill;
        beginCast(world, entity, next);
    }
    else
        cast.m_skill = kNoSkill;
}

// 强制中断当前施法。
void forceInterrupt(World& world, Entity entity)
{
    auto& book = world.ecs().get<SkillBook>(entity);
    auto& cast = world.ecs().get<SkillCast>(entity);
    if (cast.m_skill == kNoSkill)
        return;
    const auto& skill = book.m_skills[static_cast<size_t>(cast.m_skill)];
    CastEvent event;
    event.m_entity  = entity;
    event.m_skillId = skill.m_configId;
    event.m_slot    = book.m_slots[static_cast<size_t>(skill.m_slot)];
    event.m_phase   = CastPhase::Cancelled;
    world.ecs().singleton<EventQueue<CastEvent>>().push(event);
    cast.m_skill     = kNoSkill;
    cast.m_nextSkill = kNoSkill;
    cast.m_interrupted = true;
}
}  // namespace skill::rules
}  // namespace mugen::sim
