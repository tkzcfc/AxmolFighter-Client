#include "BuffTestSupport.h"

using namespace buff_test;
using namespace mugen;
using namespace mugen::sim;
using namespace mugen::conf::battle;

namespace
{
BuffOp applyOp(const std::vector<int32_t>& ids, ApplyTo to = ApplyTo::Self)
{
    BuffOp op;
    op.m_kind = BuffOpKind::Apply;
    op.m_ids  = ids;
    op.m_to   = to;
    return op;
}

BuffOp removeOp(RemoveBy by, const std::vector<int32_t>& ids)
{
    BuffOp op;
    op.m_kind = BuffOpKind::Remove;
    op.m_by   = by;
    op.m_ids  = ids;
    return op;
}
}  // namespace

TEST_CASE("buff op Stat registers and revokes a mod")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.m_persistent.push_back(statOp(BuffStat::Atk, constVal(Fixed::fromInt(7))));
    const int32_t id = h.add(def);
    h.admit(id);
    CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(7));
    h.remove(id);
    CHECK(statSum(h.world, h.self, 5) == Fixed{});
}

TEST_CASE("buff op Flag refcount SuperArmor Stun Frozen")
{
    Harness h;
    BuffDef armor = makeBuff(nextBuffId());
    armor.m_persistent.push_back(flagOp(BuffFlag::SuperArmor));
    const int32_t a = h.add(armor);
    BuffDef stun = makeBuff(nextBuffId());
    stun.m_persistent.push_back(flagOp(BuffFlag::Stun));
    const int32_t s = h.add(stun);
    h.admit(a);
    h.admit(s);
    CHECK(hasFlag(h.world, h.self, ExtraState::SuperArmor));
    CHECK(hasFlag(h.world, h.self, ExtraState::Stun));
    CHECK_FALSE(h.world.ecs().get<StatusFlagsComponent>(h.self).canMove());
    h.remove(a);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::SuperArmor));
    CHECK(hasFlag(h.world, h.self, ExtraState::Stun));
    h.remove(s);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Stun));
}

TEST_CASE("buff op SkillMod registers a scoped modifier")
{
    Harness h;
    auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
    book.m_slots  = {"A"};
    book.m_skills = {SkillRuntime{}};
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind  = BuffOpKind::SkillMod;
    op.m_field = SkillModField::CdScale;
    op.m_scope = SkillScope::All;
    op.m_value = constVal("0.5"_fx);
    def.m_persistent.push_back(op);
    const int32_t id = h.add(def);
    h.admit(id);
    REQUIRE(book.m_mods.size() == 1);
    CHECK(book.m_mods[0].m_field == static_cast<int32_t>(SkillModField::CdScale));
    CHECK(book.m_mods[0].m_value == "0.5"_fx);
    h.remove(id);
    CHECK(book.m_mods.empty());
}

TEST_CASE("buff op Immune All/Rules/BuffTypes")
{
    Harness h;
    BuffDef victim = makeBuff(nextBuffId());
    victim.m_admission.m_buffType = 3;
    victim.m_persistent.push_back(flagOp(BuffFlag::Invincible));
    const int32_t victimId = h.add(victim);

    SUBCASE("All")
    {
        BuffDef immune = makeBuff(nextBuffId());
        BuffOp op;
        op.m_kind   = BuffOpKind::Immune;
        op.m_immune = ImmuneSel::All;
        immune.m_persistent.push_back(op);
        h.admit(h.add(immune));
        h.admit(victimId);
        CHECK_FALSE(findBuff(h.world, h.self, victimId));
        CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Invincible));
    }
    SUBCASE("Rules")
    {
        BuffDef immune = makeBuff(nextBuffId());
        BuffOp op;
        op.m_kind   = BuffOpKind::Immune;
        op.m_immune = ImmuneSel::Rules;
        op.m_ids    = {victim.m_admission.m_ruleId};
        immune.m_persistent.push_back(op);
        h.admit(h.add(immune));
        h.admit(victimId);
        CHECK_FALSE(findBuff(h.world, h.self, victimId));
    }
    SUBCASE("BuffTypes")
    {
        BuffDef immune = makeBuff(nextBuffId());
        BuffOp op;
        op.m_kind   = BuffOpKind::Immune;
        op.m_immune = ImmuneSel::BuffTypes;
        op.m_ids    = {3};
        immune.m_persistent.push_back(op);
        h.admit(h.add(immune));
        h.admit(victimId);
        CHECK_FALSE(findBuff(h.world, h.self, victimId));
    }
}

TEST_CASE("buff op HpLock clamps applyDelta")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind  = BuffOpKind::HpLock;
    op.m_value = constVal(Fixed::fromInt(4000));
    def.m_persistent.push_back(op);
    h.admit(h.add(def));
    CHECK(statSum(h.world, h.self, kHpLockStat) == Fixed::fromInt(4000));
    health::rules::applyDelta(h.world, h.self, Fixed::fromInt(-9000), HpCause::Hit, h.self);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).m_hp == Fixed::fromInt(4000));
}

TEST_CASE("buff op Shield absorbs then revokes")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind  = BuffOpKind::Shield;
    op.m_value = constVal(Fixed::fromInt(30));
    def.m_persistent.push_back(op);
    const int32_t id = h.add(def);
    h.admit(id);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).m_shield == Fixed::fromInt(30));
    health::rules::applyDelta(h.world, h.self, Fixed::fromInt(-10), HpCause::Hit, h.self);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).m_shield == Fixed::fromInt(20));
    CHECK(h.world.ecs().get<HealthComponent>(h.self).m_hp == Fixed::fromInt(10000));
    h.remove(id);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).m_shield == Fixed{});
}

TEST_CASE("buff op Taunt records applicator weight")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind  = BuffOpKind::Taunt;
    op.m_value = constVal(Fixed::fromInt(8));
    def.m_persistent.push_back(op);
    BuffContext ctx;
    ctx.m_applicator = h.other;
    h.admit(h.add(def), ctx);
    const auto& table = h.world.ecs().get<TauntTableComponent>(h.self);
    REQUIRE(table.m_entries.size() == 1);
    CHECK(table.m_entries[0].m_applicator == h.other);
    CHECK(table.m_entries[0].m_weight == Fixed::fromInt(8));
}

TEST_CASE("buff op Kids admit on latch and remove on revoke")
{
    Harness h;
    BuffDef child = makeBuff(nextBuffId());
    child.m_persistent.push_back(flagOp(BuffFlag::Invincible));
    const int32_t childId = h.add(child);
    BuffDef parent = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::Kids;
    op.m_ids  = {childId};
    parent.m_persistent.push_back(op);
    const int32_t parentId = h.add(parent);
    h.admit(parentId);
    CHECK(findBuff(h.world, h.self, childId) != nullptr);
    CHECK(hasFlag(h.world, h.self, ExtraState::Invincible));
    h.remove(parentId);
    CHECK(findBuff(h.world, h.self, childId) == nullptr);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Invincible));
}

TEST_CASE("buff op Visual emits CueEvent")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind   = BuffOpKind::Visual;
    op.m_visual = VisualKind::WhiteFrame;
    def.m_persistent.push_back(op);
    h.admit(h.add(def));
    bool found = false;
    for (const auto& cue : h.world.ecs().singleton<EventQueue<CueEvent>>().m_items)
        if (cue.m_kind == CueKind::WhiteFrame && cue.m_param == 1)
            found = true;
    CHECK(found);
}

TEST_CASE("buff op Dot applies standard hurt times coeff")
{
    Harness h;
    const Fixed hp = h.world.ecs().get<HealthComponent>(h.self).m_hp;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind  = BuffOpKind::Dot;
    op.m_value = constVal("-1"_fx);
    def.m_onEnter.push_back(op);
    h.admit(h.add(def));
    const Fixed after = h.world.ecs().get<HealthComponent>(h.self).m_hp;
    CHECK(after < hp);
}

TEST_CASE("buff op Resource Hp Mp Ep Anger")
{
    Harness h;
    auto& health = h.world.ecs().get<HealthComponent>(h.self);
    health.m_hp = Fixed::fromInt(5000);
    health.m_mp = Fixed::fromInt(40);
    health.m_ep = Fixed::fromInt(100);
    h.world.ecs().get<RageStateComponent>(h.self).m_rage = Fixed::fromInt(3);

    SUBCASE("Hp")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(250))));
        h.admit(h.add(def));
        CHECK(h.world.ecs().get<HealthComponent>(h.self).m_hp == Fixed::fromInt(5250));
    }
    SUBCASE("Mp")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(resourceOp(ResourceId::Mp, constVal(Fixed::fromInt(15))));
        h.admit(h.add(def));
        CHECK(h.world.ecs().get<HealthComponent>(h.self).m_mp == Fixed::fromInt(55));
    }
    SUBCASE("Ep")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(resourceOp(ResourceId::Ep, constVal(Fixed::fromInt(20))));
        h.admit(h.add(def));
        CHECK(h.world.ecs().get<HealthComponent>(h.self).m_ep == Fixed::fromInt(120));
    }
    SUBCASE("Anger")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(resourceOp(ResourceId::Anger, constVal(Fixed::fromInt(4))));
        h.admit(h.add(def));
        CHECK(h.world.ecs().get<RageStateComponent>(h.self).m_rage == Fixed::fromInt(7));
    }
}

TEST_CASE("buff op Apply Self Applicator EventTarget Area Sequential")
{
    Harness h;
    BuffDef child = makeBuff(nextBuffId());
    child.m_persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t childId = h.add(child);

    SUBCASE("Self")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(applyOp({childId}, ApplyTo::Self));
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.self, childId) != nullptr);
        CHECK(hasFlag(h.world, h.self, ExtraState::Stealth));
    }
    SUBCASE("Applicator")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(applyOp({childId}, ApplyTo::Applicator));
        BuffContext ctx;
        ctx.m_applicator = h.other;
        h.admit(h.add(def), ctx);
        CHECK(findBuff(h.world, h.other, childId) != nullptr);
        CHECK(hasFlag(h.world, h.other, ExtraState::Stealth));
    }
    SUBCASE("EventTarget")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(applyOp({childId}, ApplyTo::EventTarget));
        BuffContext ctx;
        ctx.m_target = h.other;
        h.admit(h.add(def), ctx);
        CHECK(findBuff(h.world, h.other, childId) != nullptr);
    }
    SUBCASE("Area")
    {
        h.world.ecs().get<TransformComponent>(h.other).m_position.x = Fixed::fromInt(80);
        BuffDef def = makeBuff(nextBuffId());
        BuffOp op   = applyOp({childId}, ApplyTo::Area);
        op.m_i0     = 9;
        op.m_f0     = Fixed::fromInt(50);
        op.m_f1     = Fixed::fromInt(50);
        def.m_onEnter.push_back(op);
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.other, childId) == nullptr);
        op.m_f0 = Fixed::fromInt(200);
        BuffDef wide = makeBuff(nextBuffId());
        wide.m_onEnter.push_back(op);
        h.admit(h.add(wide));
        CHECK(findBuff(h.world, h.other, childId) != nullptr);
    }
    SUBCASE("Sequential")
    {
        BuffDef a = makeBuff(nextBuffId());
        a.m_persistent.push_back(flagOp(BuffFlag::Silence));
        BuffDef b = makeBuff(nextBuffId());
        b.m_persistent.push_back(flagOp(BuffFlag::Stealth));
        const int32_t idA = h.add(a);
        const int32_t idB = h.add(b);
        BuffDef def = makeBuff(nextBuffId());
        BuffOp op   = applyOp({idA, idB});
        op.m_pick   = PickMode::Sequential;
        def.m_onEnter.push_back(op);
        def.m_admission.m_resetType = 4;
        const int32_t id = h.add(def);
        h.admit(id);
        CHECK(findBuff(h.world, h.self, idA) != nullptr);
        CHECK(findBuff(h.world, h.self, idB) == nullptr);
        h.admit(id);
        CHECK(findBuff(h.world, h.self, idB) != nullptr);
    }
}

TEST_CASE("buff op Remove by Id Rule Type")
{
    Harness h;
    BuffDef mark = makeBuff(nextBuffId());
    mark.m_admission.m_buffType = 7;
    mark.m_persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t markId = h.add(mark);
    h.admit(markId);
    REQUIRE(findBuff(h.world, h.self, markId) != nullptr);

    SUBCASE("Id")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(removeOp(RemoveBy::Id, {markId}));
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.self, markId) == nullptr);
    }
    SUBCASE("Rule")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(removeOp(RemoveBy::Rule, {mark.m_admission.m_ruleId}));
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.self, markId) == nullptr);
    }
    SUBCASE("Type")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_onEnter.push_back(removeOp(RemoveBy::Type, {7}));
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.self, markId) == nullptr);
    }
}

TEST_CASE("buff op Count writes register used by Reg")
{
    Harness h;
    BuffDef mark = makeBuff(nextBuffId());
    mark.m_admission.m_buffType = 11;
    const int32_t markId = h.add(mark);
    h.admit(markId);
    BuffDef def = makeBuff(nextBuffId());
    BuffOp count;
    count.m_kind    = BuffOpKind::Count;
    count.m_countBy = CountBy::Type;
    count.m_i0      = 11;
    ValueExpr reg;
    reg.m_kind = ValueKind::Reg;
    reg.m_a    = Fixed::fromInt(100);
    def.m_onEnter.push_back(count);
    def.m_onEnter.push_back(statOp(BuffStat::Atk, reg));
    h.admit(h.add(def));
    CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(100));
}

TEST_CASE("buff op Cooldown Reset Add Reduce SetRatio")
{
    Harness h;
    auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
    book.m_slots = {"A"};
    SkillRuntime skill;
    skill.m_slot        = 0;
    skill.m_configId    = 920000;
    skill.m_timing      = true;
    skill.m_cd          = Fixed::fromInt(4000);
    skill.m_coldMaxTime = Fixed::fromInt(8000);
    book.m_skills.push_back(skill);

    auto admitCd = [&](CdMode mode, ValueExpr value) {
        BuffDef def = makeBuff(nextBuffId());
        BuffOp op;
        op.m_kind     = BuffOpKind::Cooldown;
        op.m_cdTarget = CdTarget::All;
        op.m_cdMode   = mode;
        op.m_value    = value;
        def.m_onEnter.push_back(op);
        h.admit(h.add(def));
    };

    SUBCASE("Reset")
    {
        admitCd(CdMode::Reset, {});
        CHECK(book.m_skills[0].m_cd == Fixed{});
        CHECK_FALSE(book.m_skills[0].m_timing);
    }
    SUBCASE("Add")
    {
        admitCd(CdMode::Add, constVal(Fixed::fromInt(2)));
        CHECK(book.m_skills[0].m_cd == Fixed::fromInt(6000));
    }
    SUBCASE("Reduce")
    {
        admitCd(CdMode::Reduce, constVal(Fixed::fromInt(1)));
        CHECK(book.m_skills[0].m_cd == Fixed::fromInt(3000));
    }
    SUBCASE("SetRatio")
    {
        admitCd(CdMode::SetRatio, constVal("0.25"_fx));
        CHECK(book.m_skills[0].m_cd == Fixed::fromInt(2000));
    }
}

TEST_CASE("buff op Extend raises maxTimes")
{
    Harness h;
    BuffDef timed = makeBuff(nextBuffId());
    timed.m_lifecycle.m_times       = 2;
    timed.m_lifecycle.m_intervalSec = Fixed::fromInt(10);
    const int32_t timedId = h.add(timed);
    h.admit(timedId);
    REQUIRE(findBuff(h.world, h.self, timedId) != nullptr);
    CHECK(findBuff(h.world, h.self, timedId)->m_maxTimes == 2);
    BuffDef ext = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::Extend;
    op.m_i0   = timed.m_admission.m_ruleId;
    op.m_i1   = 5;
    ext.m_onEnter.push_back(op);
    h.admit(h.add(ext));
    CHECK(findBuff(h.world, h.self, timedId)->m_maxTimes == 7);
}

TEST_CASE("buff op Ability grants an InitAbility instance")
{
    Harness h;
    AbilityDef ability;
    ability.m_id          = nextAbilityId();
    ability.m_status      = LowerStatus::Lowered;
    ability.m_events      = {TriggerPoint::InitAbility};
    ability.m_probability = Fixed::fromInt(100);
    ability.m_subType     = -1;
    ability.m_intervalSec = Fixed::fromInt(-1);
    BuffOp sound;
    sound.m_kind = BuffOpKind::Sound;
    sound.m_i0   = 77;
    ability.m_ops.push_back(sound);
    const int32_t abilityId = h.addAbility(ability);
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::Ability;
    op.m_i0   = abilityId;
    def.m_onEnter.push_back(op);
    h.admit(h.add(def));
    CHECK(h.world.ecs().get<AbilityListComponent>(h.self).m_instances.size() == 1);
    bool soundCue = false;
    for (const auto& cue : h.world.ecs().singleton<EventQueue<CueEvent>>().m_items)
        if (cue.m_kind == CueKind::Sound && cue.m_id == 77)
            soundCue = true;
    CHECK(soundCue);
}

TEST_CASE("buff op SpawnEffect queues a SpawnRequest")
{
    Harness h;
    int32_t effectId = 0;
    const auto& effects = conf::Config::instance().table<EffectDef>();
    REQUIRE_FALSE(effects.empty());
    effectId = effects.front().id;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::SpawnEffect;
    op.m_ids  = {effectId};
    def.m_onEnter.push_back(op);
    h.admit(h.add(def));
    bool found = false;
    for (const auto& req : h.world.ecs().singleton<EventQueue<SpawnRequest>>().m_items)
        if (req.m_kind == SpawnKind::Effect && req.m_configId == effectId)
            found = true;
    CHECK(found);
}

TEST_CASE("buff op EnterCrazy sets SkillCastComponent flag")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::EnterCrazy;
    def.m_onEnter.push_back(op);
    h.admit(h.add(def));
    CHECK(h.world.ecs().get<SkillCastComponent>(h.self).m_crazy);
}

TEST_CASE("buff op Interrupt force-cancels a cast")
{
    Harness h;
    auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
    book.m_slots = {"A"};
    SkillRuntime skill;
    skill.m_slot     = 0;
    skill.m_configId = 920000;
    book.m_skills.push_back(skill);
    h.world.ecs().get<SkillCastComponent>(h.self).m_skill = 0;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::Interrupt;
    def.m_onEnter.push_back(op);
    h.admit(h.add(def));
    CHECK(h.world.ecs().get<SkillCastComponent>(h.self).m_skill == kNoSkill);
    CHECK(h.world.ecs().get<SkillCastComponent>(h.self).m_interrupted);
}

TEST_CASE("buff op SetCost is a no-op but still admits")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::SetCost;
    op.m_res  = ResourceId::Crystal;
    op.m_value = constVal(Fixed::fromInt(1));
    def.m_onEnter.push_back(op);
    const int32_t id = h.add(def);
    h.admit(id);
    CHECK(findBuff(h.world, h.self, id) != nullptr);
}

TEST_CASE("buff op Sound emits CueEvent")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::Sound;
    op.m_i0   = 42;
    def.m_onEnter.push_back(op);
    h.admit(h.add(def));
    bool found = false;
    for (const auto& cue : h.world.ecs().singleton<EventQueue<CueEvent>>().m_items)
        if (cue.m_kind == CueKind::Sound && cue.m_id == 42)
            found = true;
    CHECK(found);
}

TEST_CASE("buff op ExitSelf destroys the instance during Enter")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.m_kind = BuffOpKind::ExitSelf;
    def.m_onEnter.push_back(op);
    const int32_t id = h.add(def);
    h.admit(id);
    CHECK(findBuff(h.world, h.self, id) == nullptr);
    CHECK(eventCount(h.world, BuffPhase::Removed, id) == 1);
}

TEST_CASE("buff op Choice runs the first weighted branch")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp choice;
    choice.m_kind        = BuffOpKind::Choice;
    choice.m_choiceBegin = 0;
    choice.m_ids2        = {1, 1};
    choice.m_table       = {Fixed::fromInt(1), Fixed::fromInt(1)};
    def.m_onEnter.push_back(choice);
    BuffOp a = statOp(BuffStat::Atk, constVal(Fixed::fromInt(3)));
    BuffOp b = statOp(BuffStat::Atk, constVal(Fixed::fromInt(9)));
    def.m_choicePool.push_back(a);
    def.m_choicePool.push_back(b);
    h.admit(h.add(def));
    CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(3));
}

TEST_CASE("buff op Watch Latched vs Edge")
{
    Harness h;
    BuffDef child = makeBuff(nextBuffId());
    child.m_persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t childId = h.add(child);
    Predicate pred;
    pred.m_kind = PredKind::HasFlag;
    pred.m_id   = static_cast<int32_t>(BuffFlag::SuperArmor);

    auto makeWatch = [&](WatchMode mode) {
        BuffDef def = makeBuff(nextBuffId());
        def.m_lifecycle.m_times = -1;
        BuffOp watch;
        watch.m_kind        = BuffOpKind::Watch;
        watch.m_watch       = mode;
        watch.m_predicates  = {pred};
        watch.m_ids         = {childId};
        def.m_watch.push_back(watch);
        return h.add(def);
    };

    SUBCASE("Latched only acts on change")
    {
        h.world.ecs().get<StatusFlagsComponent>(h.self).addFlag(BuffFlag::SuperArmor);
        const int32_t id = makeWatch(WatchMode::Latched);
        h.admit(id);
        h.tickBuff();
        CHECK(findBuff(h.world, h.self, childId) == nullptr);
        h.world.ecs().get<StatusFlagsComponent>(h.self).removeFlag(BuffFlag::SuperArmor);
        h.tickBuff();
        CHECK(findBuff(h.world, h.self, childId) != nullptr);
        CHECK(stackOf(h.world, h.self, childId) == 1);
        h.tickBuff();
        CHECK(stackOf(h.world, h.self, childId) == 1);
        h.world.ecs().get<StatusFlagsComponent>(h.self).addFlag(BuffFlag::SuperArmor);
        h.tickBuff();
        CHECK(findBuff(h.world, h.self, childId) == nullptr);
    }
    SUBCASE("Edge reapplies every tick while failing")
    {
        child.m_admission.m_subType = -1;
        // child 已注册，覆盖子类型后再写回表。
        h.buffs.add(child);
        const int32_t id = makeWatch(WatchMode::Edge);
        h.admit(id);
        h.tickBuff();
        auto countKids = [&]() {
            int32_t n = 0;
            for (const auto& inst : h.world.ecs().get<BuffListComponent>(h.self).m_instances)
                if (inst.m_id == childId && !inst.m_removing)
                    ++n;
            return n;
        };
        CHECK(countKids() == 1);
        h.tickBuff();
        CHECK(countKids() == 2);
    }
}

TEST_CASE("buff op StateTimer admits kids after threshold")
{
    Harness h;
    h.setTickSeconds(1);
    BuffDef child = makeBuff(nextBuffId());
    child.m_persistent.push_back(flagOp(BuffFlag::Silence));
    const int32_t childId = h.add(child);
    BuffDef def = makeBuff(nextBuffId());
    def.m_lifecycle.m_times = -1;
    BuffOp timer;
    timer.m_kind  = BuffOpKind::StateTimer;
    timer.m_state = StateKind::GetUp;
    timer.m_f0    = Fixed::fromInt(2);
    timer.m_ids   = {childId};
    def.m_watch.push_back(timer);
    h.admit(h.add(def));
    h.world.ecs().get<RoleStateComponent>(h.self).m_state = RoleStateId::GetUp;
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, childId) == nullptr);
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, childId) != nullptr);
    CHECK(hasFlag(h.world, h.self, ExtraState::Silence));
}

TEST_CASE("buff op SummonLink and OnSummonDeath")
{
    Harness h;
    BuffDef pulse = makeBuff(nextBuffId());
    pulse.m_persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t pulseId = h.add(pulse);
    BuffDef link = makeBuff(nextBuffId());
    BuffOp summon;
    summon.m_kind = BuffOpKind::SummonLink;
    link.m_persistent.push_back(summon);
    BuffOp death;
    death.m_kind = BuffOpKind::OnSummonDeath;
    death.m_ids  = {pulseId};
    link.m_onEnter.push_back(death);
    const int32_t linkId = h.add(link);
    h.admit(linkId);
    REQUIRE(findBuff(h.world, h.self, linkId) != nullptr);
    CHECK(findBuff(h.world, h.self, pulseId) == nullptr);
    buff::rules::notifySummonDeath(h.world, h.self);
    CHECK(findBuff(h.world, h.self, linkId) == nullptr);
    CHECK(findBuff(h.world, h.self, pulseId) != nullptr);
}

TEST_CASE("buff ValueExpr kinds")
{
    Harness h;
    auto admitStat = [&](ValueExpr expr) {
        BuffDef def = makeBuff(nextBuffId());
        def.m_persistent.push_back(statOp(BuffStat::Atk, expr));
        h.admit(h.add(def));
        return statSum(h.world, h.self, 5);
    };

    SUBCASE("Const")
    {
        CHECK(admitStat(constVal(Fixed::fromInt(12))) == Fixed::fromInt(12));
    }
    SUBCASE("PerStack")
    {
        BuffDef def = makeBuff(nextBuffId());
        ValueExpr expr;
        expr.m_kind = ValueKind::PerStack;
        expr.m_a    = Fixed::fromInt(4);
        def.m_persistent.push_back(statOp(BuffStat::Atk, expr, true));
        def.m_admission.m_resetType = 4;
        const int32_t id = h.add(def);
        h.admit(id);
        h.admit(id);
        CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(8));
    }
    SUBCASE("MaxRatio")
    {
        ValueExpr expr;
        expr.m_kind = ValueKind::MaxRatio;
        expr.m_res  = ResourceId::Hp;
        expr.m_a    = "0.5"_fx;
        CHECK(admitStat(expr) == Fixed::fromInt(5000));
    }
    SUBCASE("HpAmount")
    {
        ValueExpr expr;
        expr.m_kind = ValueKind::HpAmount;
        expr.m_a    = "0.5"_fx;
        CHECK(admitStat(expr) == Fixed::fromInt(5000));
    }
    SUBCASE("HpAmount floor")
    {
        h.world.ecs().get<HealthComponent>(h.self).m_hp = Fixed::fromInt(9000);
        ValueExpr expr;
        expr.m_kind = ValueKind::HpAmount;
        expr.m_a    = Fixed::fromInt(-50);
        expr.m_b    = Fixed::fromInt(9500);
        CHECK(admitStat(expr) == Fixed::fromInt(500));
    }
    SUBCASE("BaseMaxHp")
    {
        ValueExpr expr;
        expr.m_kind = ValueKind::BaseMaxHp;
        expr.m_a    = "0.5"_fx;
        CHECK(admitStat(expr) == Fixed::fromInt(5000));
    }
    SUBCASE("Lost")
    {
        h.world.ecs().get<HealthComponent>(h.self).m_hp = Fixed::fromInt(2500);
        ValueExpr expr;
        expr.m_kind = ValueKind::Lost;
        expr.m_res  = ResourceId::Hp;
        expr.m_a    = Fixed::fromInt(1);
        CHECK(admitStat(expr) == "0.75"_fx);
    }
    SUBCASE("Cur")
    {
        h.world.ecs().get<HealthComponent>(h.self).m_hp = Fixed::fromInt(2500);
        ValueExpr expr;
        expr.m_kind = ValueKind::Cur;
        expr.m_res  = ResourceId::Hp;
        expr.m_a    = Fixed::fromInt(1);
        CHECK(admitStat(expr) == "0.25"_fx);
    }
    SUBCASE("CurMul")
    {
        h.world.ecs().get<HealthComponent>(h.self).m_hp = Fixed::fromInt(40);
        ValueExpr expr;
        expr.m_kind = ValueKind::CurMul;
        expr.m_res  = ResourceId::Hp;
        expr.m_a    = "0.5"_fx;
        CHECK(admitStat(expr) == Fixed::fromInt(20));
    }
    SUBCASE("HpMaxAdjust")
    {
        h.world.ecs().get<HealthComponent>(h.self).m_hp = Fixed::fromInt(10000);
        ValueExpr expr;
        expr.m_kind = ValueKind::HpMaxAdjust;
        expr.m_a    = Fixed::fromInt(1);
        CHECK(admitStat(expr) == Fixed::fromInt(-5000));
    }
    SUBCASE("SpeedBonus")
    {
        BuffDef speed = makeBuff(nextBuffId());
        speed.m_persistent.push_back(statOp(BuffStat::MoveScaleX, constVal("0.5"_fx)));
        h.admit(h.add(speed));
        ValueExpr expr;
        expr.m_kind = ValueKind::SpeedBonus;
        expr.m_a    = "0.5"_fx;
        expr.m_b    = Fixed::fromInt(1);
        expr.m_c    = Fixed::fromInt(1);
        CHECK(admitStat(expr) == Fixed::fromInt(1));
    }
    SUBCASE("TypeCount")
    {
        BuffDef mark = makeBuff(nextBuffId());
        mark.m_admission.m_buffType = 21;
        h.admit(h.add(mark));
        ValueExpr expr;
        expr.m_kind  = ValueKind::TypeCount;
        expr.m_a     = Fixed::fromInt(3);
        expr.m_b     = Fixed::fromInt(10);
        expr.m_types = {21};
        CHECK(admitStat(expr) == Fixed::fromInt(3));
    }
    SUBCASE("MaxTimes")
    {
        BuffDef mark = makeBuff(nextBuffId());
        mark.m_admission.m_buffType = 22;
        h.admit(h.add(mark));
        ValueExpr expr;
        expr.m_kind  = ValueKind::MaxTimes;
        expr.m_res   = ResourceId::Hp;
        expr.m_a     = "0.5"_fx;
        expr.m_types = {22};
        CHECK(admitStat(expr) == Fixed::fromInt(5000));
    }
    SUBCASE("CoolingSlots")
    {
        auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
        book.m_slots = {"A", "B"};
        SkillRuntime a;
        a.m_slot   = 0;
        a.m_timing = true;
        a.m_cd     = Fixed::fromInt(1);
        SkillRuntime b;
        b.m_slot   = 1;
        b.m_timing = true;
        b.m_cd     = Fixed::fromInt(1);
        book.m_skills = {a, b};
        ValueExpr expr;
        expr.m_kind = ValueKind::CoolingSlots;
        expr.m_a    = Fixed::fromInt(5);
        expr.m_b    = Fixed::fromInt(100);
        CHECK(admitStat(expr) == Fixed::fromInt(10));
    }
    SUBCASE("CoolingSlotTable")
    {
        auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
        book.m_slots = {"A"};
        SkillRuntime a;
        a.m_slot   = 0;
        a.m_timing = true;
        a.m_cd     = Fixed::fromInt(1);
        book.m_skills = {a};
        ValueExpr expr;
        expr.m_kind  = ValueKind::CoolingSlotTable;
        expr.m_table = {Fixed::fromInt(0), Fixed::fromInt(17)};
        CHECK(admitStat(expr) == Fixed::fromInt(17));
    }
    SUBCASE("ComboTable")
    {
        h.world.ecs().get<ComboMeterComponent>(h.self).m_grade = 2;
        ValueExpr expr;
        expr.m_kind  = ValueKind::ComboTable;
        expr.m_table = {Fixed::fromInt(1), Fixed::fromInt(2), Fixed::fromInt(9)};
        expr.m_bias  = Fixed::fromInt(-1);
        CHECK(admitStat(expr) == Fixed::fromInt(8));
    }
    SUBCASE("LastDamage")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.m_lifecycle.m_executeType = 3;
        def.m_activation.m_beginOn    = TriggerPoint::AfterHit;
        def.m_activation.m_latchOn    = static_cast<int32_t>(BuffHook::Begin);
        ValueExpr expr;
        expr.m_kind = ValueKind::LastDamage;
        expr.m_a    = Fixed::fromInt(2);
        def.m_persistent.push_back(statOp(BuffStat::Atk, expr));
        const int32_t id = h.add(def);
        h.admit(id);
        BuffContext ctx;
        ctx.m_damage = Fixed::fromInt(40);
        h.fire(TriggerPoint::AfterHit, ctx);
        CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(80));
    }
    SUBCASE("RegOfMax")
    {
        BuffDef def = makeBuff(nextBuffId());
        BuffOp count;
        count.m_kind    = BuffOpKind::Count;
        count.m_countBy = CountBy::Id;
        count.m_i0      = -1;
        ValueExpr expr;
        expr.m_kind = ValueKind::RegOfMax;
        expr.m_res  = ResourceId::Hp;
        expr.m_a    = "0.5"_fx;
        // Count 找不到 id，寄存器为 0；先挂一个同 rule 的标记再 Count 自身。
        BuffDef mark = makeBuff(nextBuffId());
        const int32_t markId = h.add(mark);
        h.admit(markId);
        count.m_i0 = markId;
        count.m_countBy = CountBy::Id;
        def.m_onEnter.push_back(count);
        def.m_onEnter.push_back(statOp(BuffStat::Atk, expr));
        h.admit(h.add(def));
        CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(5000));
    }
}

TEST_CASE("buff op Stat live replaces the mod each Tick")
{
    Harness h;
    h.setTickSeconds(1);
    h.world.ecs().get<HealthComponent>(h.self).m_hp = Fixed::fromInt(5000);
    BuffDef def = makeBuff(nextBuffId());
    def.m_lifecycle.m_times = -1;
    def.m_activation.m_latchOn =
        static_cast<int32_t>(BuffHook::Enter) | static_cast<int32_t>(BuffHook::Tick);
    ValueExpr expr;
    expr.m_kind = ValueKind::Cur;
    expr.m_res  = ResourceId::Hp;
    expr.m_a    = Fixed::fromInt(1);
    def.m_persistent.push_back(statOp(BuffStat::Atk, expr, true));
    h.admit(h.add(def));
    CHECK(statSum(h.world, h.self, 5) == "0.5"_fx);
    h.world.ecs().get<HealthComponent>(h.self).m_hp = Fixed::fromInt(2500);
    h.tickBuff();
    CHECK(statSum(h.world, h.self, 5) == "0.25"_fx);
}
