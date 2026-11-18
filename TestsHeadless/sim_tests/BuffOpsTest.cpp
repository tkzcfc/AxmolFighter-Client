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
    op.kind = BuffOpKind::Apply;
    op.ids  = ids;
    op.to   = to;
    return op;
}

BuffOp removeOp(RemoveBy by, const std::vector<int32_t>& ids)
{
    BuffOp op;
    op.kind = BuffOpKind::Remove;
    op.by   = by;
    op.ids  = ids;
    return op;
}
}  // namespace

TEST_CASE("buff op Stat registers and revokes a mod")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.persistent.push_back(statOp(BuffStat::Atk, constVal(Fixed::fromInt(7))));
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
    armor.persistent.push_back(flagOp(BuffFlag::SuperArmor));
    const int32_t a = h.add(armor);
    BuffDef stun = makeBuff(nextBuffId());
    stun.persistent.push_back(flagOp(BuffFlag::Stun));
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
    book.slots  = {"A"};
    book.skills = {SkillRuntime{}};
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind  = BuffOpKind::SkillMod;
    op.field = SkillModField::CdScale;
    op.scope = SkillScope::All;
    op.value = constVal("0.5"_fx);
    def.persistent.push_back(op);
    const int32_t id = h.add(def);
    h.admit(id);
    REQUIRE(book.mods.size() == 1);
    CHECK(book.mods[0].field == static_cast<int32_t>(SkillModField::CdScale));
    CHECK(book.mods[0].value == "0.5"_fx);
    h.remove(id);
    CHECK(book.mods.empty());
}

TEST_CASE("buff op Immune All/Rules/BuffTypes")
{
    Harness h;
    BuffDef victim = makeBuff(nextBuffId());
    victim.admission.buffType = 3;
    victim.persistent.push_back(flagOp(BuffFlag::Invincible));
    const int32_t victimId = h.add(victim);

    SUBCASE("All")
    {
        BuffDef immune = makeBuff(nextBuffId());
        BuffOp op;
        op.kind   = BuffOpKind::Immune;
        op.immune = ImmuneSel::All;
        immune.persistent.push_back(op);
        h.admit(h.add(immune));
        h.admit(victimId);
        CHECK_FALSE(findBuff(h.world, h.self, victimId));
        CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Invincible));
    }
    SUBCASE("Rules")
    {
        BuffDef immune = makeBuff(nextBuffId());
        BuffOp op;
        op.kind   = BuffOpKind::Immune;
        op.immune = ImmuneSel::Rules;
        op.ids    = {victim.admission.ruleId};
        immune.persistent.push_back(op);
        h.admit(h.add(immune));
        h.admit(victimId);
        CHECK_FALSE(findBuff(h.world, h.self, victimId));
    }
    SUBCASE("BuffTypes")
    {
        BuffDef immune = makeBuff(nextBuffId());
        BuffOp op;
        op.kind   = BuffOpKind::Immune;
        op.immune = ImmuneSel::BuffTypes;
        op.ids    = {3};
        immune.persistent.push_back(op);
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
    op.kind  = BuffOpKind::HpLock;
    op.value = constVal(Fixed::fromInt(4000));
    def.persistent.push_back(op);
    h.admit(h.add(def));
    CHECK(statSum(h.world, h.self, kHpLockStat) == Fixed::fromInt(4000));
    health::rules::applyDelta(h.world, h.self, Fixed::fromInt(-9000), HpCause::Hit, h.self);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == Fixed::fromInt(4000));
}

TEST_CASE("buff op Shield absorbs then revokes")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind  = BuffOpKind::Shield;
    op.value = constVal(Fixed::fromInt(30));
    def.persistent.push_back(op);
    const int32_t id = h.add(def);
    h.admit(id);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).shield == Fixed::fromInt(30));
    health::rules::applyDelta(h.world, h.self, Fixed::fromInt(-10), HpCause::Hit, h.self);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).shield == Fixed::fromInt(20));
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == Fixed::fromInt(10000));
    h.remove(id);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).shield == Fixed{});
}

TEST_CASE("buff op Taunt records applicator weight")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind  = BuffOpKind::Taunt;
    op.value = constVal(Fixed::fromInt(8));
    def.persistent.push_back(op);
    BuffContext ctx;
    ctx.applicator = h.other;
    h.admit(h.add(def), ctx);
    const auto& table = h.world.ecs().get<TauntTableComponent>(h.self);
    REQUIRE(table.entries.size() == 1);
    CHECK(table.entries[0].applicator == h.other);
    CHECK(table.entries[0].weight == Fixed::fromInt(8));
}

TEST_CASE("buff op Kids admit on latch and remove on revoke")
{
    Harness h;
    BuffDef child = makeBuff(nextBuffId());
    child.persistent.push_back(flagOp(BuffFlag::Invincible));
    const int32_t childId = h.add(child);
    BuffDef parent = makeBuff(nextBuffId());
    BuffOp op;
    op.kind = BuffOpKind::Kids;
    op.ids  = {childId};
    parent.persistent.push_back(op);
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
    op.kind   = BuffOpKind::Visual;
    op.visual = VisualKind::WhiteFrame;
    def.persistent.push_back(op);
    h.admit(h.add(def));
    bool found = false;
    for (const auto& cue : h.world.ecs().singleton<EventQueue<CueEvent>>().items)
        if (cue.kind == CueKind::WhiteFrame && cue.param == 1)
            found = true;
    CHECK(found);
}

TEST_CASE("buff op Dot applies standard hurt times coeff")
{
    Harness h;
    const Fixed hp = h.world.ecs().get<HealthComponent>(h.self).hp;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind  = BuffOpKind::Dot;
    op.value = constVal("-1"_fx);
    def.onEnter.push_back(op);
    h.admit(h.add(def));
    const Fixed after = h.world.ecs().get<HealthComponent>(h.self).hp;
    CHECK(after < hp);
}

TEST_CASE("buff op Resource Hp Mp Ep Anger")
{
    Harness h;
    auto& health = h.world.ecs().get<HealthComponent>(h.self);
    health.hp = Fixed::fromInt(5000);
    health.mp = Fixed::fromInt(40);
    health.ep = Fixed::fromInt(100);
    h.world.ecs().get<RageStateComponent>(h.self).rage = Fixed::fromInt(3);

    SUBCASE("Hp")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(250))));
        h.admit(h.add(def));
        CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == Fixed::fromInt(5250));
    }
    SUBCASE("Mp")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(resourceOp(ResourceId::Mp, constVal(Fixed::fromInt(15))));
        h.admit(h.add(def));
        CHECK(h.world.ecs().get<HealthComponent>(h.self).mp == Fixed::fromInt(55));
    }
    SUBCASE("Ep")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(resourceOp(ResourceId::Ep, constVal(Fixed::fromInt(20))));
        h.admit(h.add(def));
        CHECK(h.world.ecs().get<HealthComponent>(h.self).ep == Fixed::fromInt(120));
    }
    SUBCASE("Anger")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(resourceOp(ResourceId::Anger, constVal(Fixed::fromInt(4))));
        h.admit(h.add(def));
        CHECK(h.world.ecs().get<RageStateComponent>(h.self).rage == Fixed::fromInt(7));
    }
}

TEST_CASE("buff op Apply Self Applicator EventTarget Area Sequential")
{
    Harness h;
    BuffDef child = makeBuff(nextBuffId());
    child.persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t childId = h.add(child);

    SUBCASE("Self")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(applyOp({childId}, ApplyTo::Self));
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.self, childId) != nullptr);
        CHECK(hasFlag(h.world, h.self, ExtraState::Stealth));
    }
    SUBCASE("Applicator")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(applyOp({childId}, ApplyTo::Applicator));
        BuffContext ctx;
        ctx.applicator = h.other;
        h.admit(h.add(def), ctx);
        CHECK(findBuff(h.world, h.other, childId) != nullptr);
        CHECK(hasFlag(h.world, h.other, ExtraState::Stealth));
    }
    SUBCASE("EventTarget")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(applyOp({childId}, ApplyTo::EventTarget));
        BuffContext ctx;
        ctx.target = h.other;
        h.admit(h.add(def), ctx);
        CHECK(findBuff(h.world, h.other, childId) != nullptr);
    }
    SUBCASE("Area")
    {
        h.world.ecs().get<TransformComponent>(h.other).position.x = Fixed::fromInt(80);
        BuffDef def = makeBuff(nextBuffId());
        BuffOp op   = applyOp({childId}, ApplyTo::Area);
        op.i0     = 9;
        op.f0     = Fixed::fromInt(50);
        op.f1     = Fixed::fromInt(50);
        def.onEnter.push_back(op);
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.other, childId) == nullptr);
        op.f0 = Fixed::fromInt(200);
        BuffDef wide = makeBuff(nextBuffId());
        wide.onEnter.push_back(op);
        h.admit(h.add(wide));
        CHECK(findBuff(h.world, h.other, childId) != nullptr);
    }
    SUBCASE("Sequential")
    {
        BuffDef a = makeBuff(nextBuffId());
        a.persistent.push_back(flagOp(BuffFlag::Silence));
        BuffDef b = makeBuff(nextBuffId());
        b.persistent.push_back(flagOp(BuffFlag::Stealth));
        const int32_t idA = h.add(a);
        const int32_t idB = h.add(b);
        BuffDef def = makeBuff(nextBuffId());
        BuffOp op   = applyOp({idA, idB});
        op.pick   = PickMode::Sequential;
        def.onEnter.push_back(op);
        def.admission.resetType = 4;
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
    mark.admission.buffType = 7;
    mark.persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t markId = h.add(mark);
    h.admit(markId);
    REQUIRE(findBuff(h.world, h.self, markId) != nullptr);

    SUBCASE("Id")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(removeOp(RemoveBy::Id, {markId}));
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.self, markId) == nullptr);
    }
    SUBCASE("Rule")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(removeOp(RemoveBy::Rule, {mark.admission.ruleId}));
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.self, markId) == nullptr);
    }
    SUBCASE("Type")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.onEnter.push_back(removeOp(RemoveBy::Type, {7}));
        h.admit(h.add(def));
        CHECK(findBuff(h.world, h.self, markId) == nullptr);
    }
}

TEST_CASE("buff op Count writes register used by Reg")
{
    Harness h;
    BuffDef mark = makeBuff(nextBuffId());
    mark.admission.buffType = 11;
    const int32_t markId = h.add(mark);
    h.admit(markId);
    BuffDef def = makeBuff(nextBuffId());
    BuffOp count;
    count.kind    = BuffOpKind::Count;
    count.countBy = CountBy::Type;
    count.i0      = 11;
    ValueExpr reg;
    reg.kind = ValueKind::Reg;
    reg.a    = Fixed::fromInt(100);
    def.onEnter.push_back(count);
    def.onEnter.push_back(statOp(BuffStat::Atk, reg));
    h.admit(h.add(def));
    CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(100));
}

TEST_CASE("buff op Cooldown Reset Add Reduce SetRatio")
{
    Harness h;
    auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
    book.slots = {"A"};
    SkillRuntime skill;
    skill.slot        = 0;
    skill.configId    = 920000;
    skill.timing      = true;
    skill.cd          = Fixed::fromInt(4000);
    skill.coldMaxTime = Fixed::fromInt(8000);
    book.skills.push_back(skill);

    auto admitCd = [&](CdMode mode, ValueExpr value) {
        BuffDef def = makeBuff(nextBuffId());
        BuffOp op;
        op.kind     = BuffOpKind::Cooldown;
        op.cdTarget = CdTarget::All;
        op.cdMode   = mode;
        op.value    = value;
        def.onEnter.push_back(op);
        h.admit(h.add(def));
    };

    SUBCASE("Reset")
    {
        admitCd(CdMode::Reset, {});
        CHECK(book.skills[0].cd == Fixed{});
        CHECK_FALSE(book.skills[0].timing);
    }
    SUBCASE("Add")
    {
        admitCd(CdMode::Add, constVal(Fixed::fromInt(2)));
        CHECK(book.skills[0].cd == Fixed::fromInt(6000));
    }
    SUBCASE("Reduce")
    {
        admitCd(CdMode::Reduce, constVal(Fixed::fromInt(1)));
        CHECK(book.skills[0].cd == Fixed::fromInt(3000));
    }
    SUBCASE("SetRatio")
    {
        admitCd(CdMode::SetRatio, constVal("0.25"_fx));
        CHECK(book.skills[0].cd == Fixed::fromInt(2000));
    }
}

TEST_CASE("buff op Extend raises maxTimes")
{
    Harness h;
    BuffDef timed = makeBuff(nextBuffId());
    timed.lifecycle.times       = 2;
    timed.lifecycle.intervalSec = Fixed::fromInt(10);
    const int32_t timedId = h.add(timed);
    h.admit(timedId);
    REQUIRE(findBuff(h.world, h.self, timedId) != nullptr);
    CHECK(findBuff(h.world, h.self, timedId)->maxTimes == 2);
    BuffDef ext = makeBuff(nextBuffId());
    BuffOp op;
    op.kind = BuffOpKind::Extend;
    op.i0   = timed.admission.ruleId;
    op.i1   = 5;
    ext.onEnter.push_back(op);
    h.admit(h.add(ext));
    CHECK(findBuff(h.world, h.self, timedId)->maxTimes == 7);
}

TEST_CASE("buff op Ability grants an InitAbility instance")
{
    Harness h;
    AbilityDef ability;
    ability.id          = nextAbilityId();
    ability.status      = LowerStatus::Lowered;
    ability.events      = {TriggerPoint::InitAbility};
    ability.probability = Fixed::fromInt(100);
    ability.subType     = -1;
    ability.intervalSec = Fixed::fromInt(-1);
    BuffOp sound;
    sound.kind = BuffOpKind::Sound;
    sound.i0   = 77;
    ability.ops.push_back(sound);
    const int32_t abilityId = h.addAbility(ability);
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind = BuffOpKind::Ability;
    op.i0   = abilityId;
    def.onEnter.push_back(op);
    h.admit(h.add(def));
    CHECK(h.world.ecs().get<AbilityListComponent>(h.self).instances.size() == 1);
    bool soundCue = false;
    for (const auto& cue : h.world.ecs().singleton<EventQueue<CueEvent>>().items)
        if (cue.kind == CueKind::Sound && cue.id == 77)
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
    op.kind = BuffOpKind::SpawnEffect;
    op.ids  = {effectId};
    def.onEnter.push_back(op);
    h.admit(h.add(def));
    bool found = false;
    for (const auto& req : h.world.ecs().singleton<EventQueue<SpawnRequest>>().items)
        if (req.kind == SpawnKind::Effect && req.configId == effectId)
            found = true;
    CHECK(found);
}

TEST_CASE("buff op EnterCrazy sets SkillCastComponent flag")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind = BuffOpKind::EnterCrazy;
    def.onEnter.push_back(op);
    h.admit(h.add(def));
    CHECK(h.world.ecs().get<SkillCastComponent>(h.self).crazy);
}

TEST_CASE("buff op Interrupt force-cancels a cast")
{
    Harness h;
    auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
    book.slots = {"A"};
    SkillRuntime skill;
    skill.slot     = 0;
    skill.configId = 920000;
    book.skills.push_back(skill);
    h.world.ecs().get<SkillCastComponent>(h.self).skill = 0;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind = BuffOpKind::Interrupt;
    def.onEnter.push_back(op);
    h.admit(h.add(def));
    CHECK(h.world.ecs().get<SkillCastComponent>(h.self).skill == kNoSkill);
    CHECK(h.world.ecs().get<SkillCastComponent>(h.self).interrupted);
}

TEST_CASE("buff op SetCost is a no-op but still admits")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind = BuffOpKind::SetCost;
    op.res  = ResourceId::Crystal;
    op.value = constVal(Fixed::fromInt(1));
    def.onEnter.push_back(op);
    const int32_t id = h.add(def);
    h.admit(id);
    CHECK(findBuff(h.world, h.self, id) != nullptr);
}

TEST_CASE("buff op Sound emits CueEvent")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind = BuffOpKind::Sound;
    op.i0   = 42;
    def.onEnter.push_back(op);
    h.admit(h.add(def));
    bool found = false;
    for (const auto& cue : h.world.ecs().singleton<EventQueue<CueEvent>>().items)
        if (cue.kind == CueKind::Sound && cue.id == 42)
            found = true;
    CHECK(found);
}

TEST_CASE("buff op ExitSelf destroys the instance during Enter")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    BuffOp op;
    op.kind = BuffOpKind::ExitSelf;
    def.onEnter.push_back(op);
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
    choice.kind        = BuffOpKind::Choice;
    choice.choiceBegin = 0;
    choice.ids2        = {1, 1};
    choice.table       = {Fixed::fromInt(1), Fixed::fromInt(1)};
    def.onEnter.push_back(choice);
    BuffOp a = statOp(BuffStat::Atk, constVal(Fixed::fromInt(3)));
    BuffOp b = statOp(BuffStat::Atk, constVal(Fixed::fromInt(9)));
    def.choicePool.push_back(a);
    def.choicePool.push_back(b);
    h.admit(h.add(def));
    CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(3));
}

TEST_CASE("buff op Watch Latched vs Edge")
{
    Harness h;
    BuffDef child = makeBuff(nextBuffId());
    child.persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t childId = h.add(child);
    Predicate pred;
    pred.kind = PredKind::HasFlag;
    pred.id   = static_cast<int32_t>(BuffFlag::SuperArmor);

    auto makeWatch = [&](WatchMode mode) {
        BuffDef def = makeBuff(nextBuffId());
        def.lifecycle.times = -1;
        BuffOp watch;
        watch.kind        = BuffOpKind::Watch;
        watch.watch       = mode;
        watch.predicates  = {pred};
        watch.ids         = {childId};
        def.watch.push_back(watch);
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
        child.admission.subType = -1;
        // child 已注册，覆盖子类型后再写回表。
        h.buffs.add(child);
        const int32_t id = makeWatch(WatchMode::Edge);
        h.admit(id);
        h.tickBuff();
        auto countKids = [&]() {
            int32_t n = 0;
            for (const auto& inst : h.world.ecs().get<BuffListComponent>(h.self).instances)
                if (inst.id == childId && !inst.removing)
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
    child.persistent.push_back(flagOp(BuffFlag::Silence));
    const int32_t childId = h.add(child);
    BuffDef def = makeBuff(nextBuffId());
    def.lifecycle.times = -1;
    BuffOp timer;
    timer.kind  = BuffOpKind::StateTimer;
    timer.state = StateKind::GetUp;
    timer.f0    = Fixed::fromInt(2);
    timer.ids   = {childId};
    def.watch.push_back(timer);
    h.admit(h.add(def));
    h.world.ecs().get<RoleStateComponent>(h.self).state = RoleStateId::GetUp;
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
    pulse.persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t pulseId = h.add(pulse);
    BuffDef link = makeBuff(nextBuffId());
    BuffOp summon;
    summon.kind = BuffOpKind::SummonLink;
    link.persistent.push_back(summon);
    BuffOp death;
    death.kind = BuffOpKind::OnSummonDeath;
    death.ids  = {pulseId};
    link.onEnter.push_back(death);
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
        def.persistent.push_back(statOp(BuffStat::Atk, expr));
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
        expr.kind = ValueKind::PerStack;
        expr.a    = Fixed::fromInt(4);
        def.persistent.push_back(statOp(BuffStat::Atk, expr, true));
        def.admission.resetType = 4;
        const int32_t id = h.add(def);
        h.admit(id);
        h.admit(id);
        CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(8));
    }
    SUBCASE("MaxRatio")
    {
        ValueExpr expr;
        expr.kind = ValueKind::MaxRatio;
        expr.res  = ResourceId::Hp;
        expr.a    = "0.5"_fx;
        CHECK(admitStat(expr) == Fixed::fromInt(5000));
    }
    SUBCASE("HpAmount")
    {
        ValueExpr expr;
        expr.kind = ValueKind::HpAmount;
        expr.a    = "0.5"_fx;
        CHECK(admitStat(expr) == Fixed::fromInt(5000));
    }
    SUBCASE("HpAmount floor")
    {
        h.world.ecs().get<HealthComponent>(h.self).hp = Fixed::fromInt(9000);
        ValueExpr expr;
        expr.kind = ValueKind::HpAmount;
        expr.a    = Fixed::fromInt(-50);
        expr.b    = Fixed::fromInt(9500);
        CHECK(admitStat(expr) == Fixed::fromInt(500));
    }
    SUBCASE("BaseMaxHp")
    {
        ValueExpr expr;
        expr.kind = ValueKind::BaseMaxHp;
        expr.a    = "0.5"_fx;
        CHECK(admitStat(expr) == Fixed::fromInt(5000));
    }
    SUBCASE("Lost")
    {
        h.world.ecs().get<HealthComponent>(h.self).hp = Fixed::fromInt(2500);
        ValueExpr expr;
        expr.kind = ValueKind::Lost;
        expr.res  = ResourceId::Hp;
        expr.a    = Fixed::fromInt(1);
        CHECK(admitStat(expr) == "0.75"_fx);
    }
    SUBCASE("Cur")
    {
        h.world.ecs().get<HealthComponent>(h.self).hp = Fixed::fromInt(2500);
        ValueExpr expr;
        expr.kind = ValueKind::Cur;
        expr.res  = ResourceId::Hp;
        expr.a    = Fixed::fromInt(1);
        CHECK(admitStat(expr) == "0.25"_fx);
    }
    SUBCASE("CurMul")
    {
        h.world.ecs().get<HealthComponent>(h.self).hp = Fixed::fromInt(40);
        ValueExpr expr;
        expr.kind = ValueKind::CurMul;
        expr.res  = ResourceId::Hp;
        expr.a    = "0.5"_fx;
        CHECK(admitStat(expr) == Fixed::fromInt(20));
    }
    SUBCASE("HpMaxAdjust")
    {
        h.world.ecs().get<HealthComponent>(h.self).hp = Fixed::fromInt(10000);
        ValueExpr expr;
        expr.kind = ValueKind::HpMaxAdjust;
        expr.a    = Fixed::fromInt(1);
        CHECK(admitStat(expr) == Fixed::fromInt(-5000));
    }
    SUBCASE("SpeedBonus")
    {
        BuffDef speed = makeBuff(nextBuffId());
        speed.persistent.push_back(statOp(BuffStat::MoveScaleX, constVal("0.5"_fx)));
        h.admit(h.add(speed));
        ValueExpr expr;
        expr.kind = ValueKind::SpeedBonus;
        expr.a    = "0.5"_fx;
        expr.b    = Fixed::fromInt(1);
        expr.c    = Fixed::fromInt(1);
        CHECK(admitStat(expr) == Fixed::fromInt(1));
    }
    SUBCASE("TypeCount")
    {
        BuffDef mark = makeBuff(nextBuffId());
        mark.admission.buffType = 21;
        h.admit(h.add(mark));
        ValueExpr expr;
        expr.kind  = ValueKind::TypeCount;
        expr.a     = Fixed::fromInt(3);
        expr.b     = Fixed::fromInt(10);
        expr.types = {21};
        CHECK(admitStat(expr) == Fixed::fromInt(3));
    }
    SUBCASE("MaxTimes")
    {
        BuffDef mark = makeBuff(nextBuffId());
        mark.admission.buffType = 22;
        h.admit(h.add(mark));
        ValueExpr expr;
        expr.kind  = ValueKind::MaxTimes;
        expr.res   = ResourceId::Hp;
        expr.a     = "0.5"_fx;
        expr.types = {22};
        CHECK(admitStat(expr) == Fixed::fromInt(5000));
    }
    SUBCASE("CoolingSlots")
    {
        auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
        book.slots = {"A", "B"};
        SkillRuntime a;
        a.slot   = 0;
        a.timing = true;
        a.cd     = Fixed::fromInt(1);
        SkillRuntime b;
        b.slot   = 1;
        b.timing = true;
        b.cd     = Fixed::fromInt(1);
        book.skills = {a, b};
        ValueExpr expr;
        expr.kind = ValueKind::CoolingSlots;
        expr.a    = Fixed::fromInt(5);
        expr.b    = Fixed::fromInt(100);
        CHECK(admitStat(expr) == Fixed::fromInt(10));
    }
    SUBCASE("CoolingSlotTable")
    {
        auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
        book.slots = {"A"};
        SkillRuntime a;
        a.slot   = 0;
        a.timing = true;
        a.cd     = Fixed::fromInt(1);
        book.skills = {a};
        ValueExpr expr;
        expr.kind  = ValueKind::CoolingSlotTable;
        expr.table = {Fixed::fromInt(0), Fixed::fromInt(17)};
        CHECK(admitStat(expr) == Fixed::fromInt(17));
    }
    SUBCASE("ComboTable")
    {
        h.world.ecs().get<ComboMeterComponent>(h.self).grade = 2;
        ValueExpr expr;
        expr.kind  = ValueKind::ComboTable;
        expr.table = {Fixed::fromInt(1), Fixed::fromInt(2), Fixed::fromInt(9)};
        expr.bias  = Fixed::fromInt(-1);
        CHECK(admitStat(expr) == Fixed::fromInt(8));
    }
    SUBCASE("LastDamage")
    {
        BuffDef def = makeBuff(nextBuffId());
        def.lifecycle.executeType = 3;
        def.activation.beginOn    = TriggerPoint::AfterHit;
        def.activation.latchOn    = static_cast<int32_t>(BuffHook::Begin);
        ValueExpr expr;
        expr.kind = ValueKind::LastDamage;
        expr.a    = Fixed::fromInt(2);
        def.persistent.push_back(statOp(BuffStat::Atk, expr));
        const int32_t id = h.add(def);
        h.admit(id);
        BuffContext ctx;
        ctx.damage = Fixed::fromInt(40);
        h.fire(TriggerPoint::AfterHit, ctx);
        CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(80));
    }
    SUBCASE("RegOfMax")
    {
        BuffDef def = makeBuff(nextBuffId());
        BuffOp count;
        count.kind    = BuffOpKind::Count;
        count.countBy = CountBy::Id;
        count.i0      = -1;
        ValueExpr expr;
        expr.kind = ValueKind::RegOfMax;
        expr.res  = ResourceId::Hp;
        expr.a    = "0.5"_fx;
        // Count 找不到 id，寄存器为 0；先挂一个同 rule 的标记再 Count 自身。
        BuffDef mark = makeBuff(nextBuffId());
        const int32_t markId = h.add(mark);
        h.admit(markId);
        count.i0 = markId;
        count.countBy = CountBy::Id;
        def.onEnter.push_back(count);
        def.onEnter.push_back(statOp(BuffStat::Atk, expr));
        h.admit(h.add(def));
        CHECK(statSum(h.world, h.self, 5) == Fixed::fromInt(5000));
    }
}

TEST_CASE("buff op Stat live replaces the mod each Tick")
{
    Harness h;
    h.setTickSeconds(1);
    h.world.ecs().get<HealthComponent>(h.self).hp = Fixed::fromInt(5000);
    BuffDef def = makeBuff(nextBuffId());
    def.lifecycle.times = -1;
    def.activation.latchOn =
        static_cast<int32_t>(BuffHook::Enter) | static_cast<int32_t>(BuffHook::Tick);
    ValueExpr expr;
    expr.kind = ValueKind::Cur;
    expr.res  = ResourceId::Hp;
    expr.a    = Fixed::fromInt(1);
    def.persistent.push_back(statOp(BuffStat::Atk, expr, true));
    h.admit(h.add(def));
    CHECK(statSum(h.world, h.self, 5) == "0.5"_fx);
    h.world.ecs().get<HealthComponent>(h.self).hp = Fixed::fromInt(2500);
    h.tickBuff();
    CHECK(statSum(h.world, h.self, 5) == "0.25"_fx);
}
