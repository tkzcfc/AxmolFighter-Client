#include "BuffTestSupport.h"

using namespace buff_test;
using namespace mugen;
using namespace mugen::sim;
using namespace mugen::conf::battle;

namespace
{
// 用 Enter 上带前置谓词的 Resource 探测谓词是否为真。
bool firesWhen(Harness& h, Predicate pred)
{
    const Fixed before = h.world.ecs().get<HealthComponent>(h.self).hp;
    BuffDef def        = makeBuff(nextBuffId());
    BuffOp op          = resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-1)));
    op.when          = pred;
    def.onEnter.push_back(op);
    h.admit(h.add(def));
    return h.world.ecs().get<HealthComponent>(h.self).hp < before;
}

void addMarker(Harness& h, int32_t& id, int32_t ruleId, int32_t buffType)
{
    BuffDef mark                 = makeBuff(nextBuffId());
    mark.admission.ruleId    = ruleId;
    mark.admission.buffType  = buffType;
    id                           = h.add(mark);
    h.admit(id);
}
}  // namespace

TEST_CASE("buff predicate None is true")
{
    Harness h;
    Predicate pred;
    CHECK(firesWhen(h, pred));
}

TEST_CASE("buff predicate HasRule")
{
    Harness h;
    int32_t marker = 0;
    addMarker(h, marker, 88001, 1);
    Predicate pred;
    pred.kind = PredKind::HasRule;
    pred.id   = 88001;
    CHECK(firesWhen(h, pred));
    Predicate miss;
    miss.kind = PredKind::HasRule;
    miss.id   = 88002;
    CHECK_FALSE(firesWhen(h, miss));
    Predicate all;
    all.kind  = PredKind::HasRule;
    all.allOf = true;
    all.ids   = {88001, 88002};
    CHECK_FALSE(firesWhen(h, all));
    int32_t extra = 0;
    addMarker(h, extra, 88002, 1);
    CHECK(firesWhen(h, all));
}

TEST_CASE("buff predicate HasBuff")
{
    Harness h;
    int32_t marker = 0;
    addMarker(h, marker, 88100, 1);
    Predicate pred;
    pred.kind = PredKind::HasBuff;
    pred.id   = marker;
    CHECK(firesWhen(h, pred));
    Predicate miss;
    miss.kind      = PredKind::HasBuff;
    miss.id        = marker;
    miss.minStacks = 2;
    CHECK_FALSE(firesWhen(h, miss));
}

TEST_CASE("buff predicate HasBuffType")
{
    Harness h;
    int32_t marker = 0;
    addMarker(h, marker, 88200, 9);
    Predicate pred;
    pred.kind = PredKind::HasBuffType;
    pred.id   = 9;
    CHECK(firesWhen(h, pred));
    Predicate miss;
    miss.kind = PredKind::HasBuffType;
    miss.id   = 8;
    CHECK_FALSE(firesWhen(h, miss));
}

TEST_CASE("buff predicate HasFlag")
{
    Harness h;
    h.world.ecs().get<StatusFlagsComponent>(h.self).addFlag(BuffFlag::SuperArmor);
    Predicate pred;
    pred.kind = PredKind::HasFlag;
    pred.id   = static_cast<int32_t>(BuffFlag::SuperArmor);
    CHECK(firesWhen(h, pred));
    Predicate crazy;
    crazy.kind = PredKind::HasFlag;
    crazy.id   = static_cast<int32_t>(BuffFlag::Crazy);
    CHECK_FALSE(firesWhen(h, crazy));
    h.world.ecs().get<SkillCastComponent>(h.self).crazy = true;
    CHECK(firesWhen(h, crazy));
    Predicate notCrazy;
    notCrazy.kind = PredKind::HasFlag;
    notCrazy.id   = static_cast<int32_t>(BuffFlag::NotCrazy);
    CHECK_FALSE(firesWhen(h, notCrazy));
}

TEST_CASE("buff predicate Motion")
{
    Harness h;
    auto setScale = [&](Fixed delta) {
        h.world.ecs().get<StatsComponent>(h.self).mods.clear();
        h.world.ecs().get<StatsComponent>(h.self).mods.push_back({200, delta, 0});
    };
    Predicate up;
    up.kind    = PredKind::Motion;
    up.subKind = static_cast<int32_t>(MotionCheck::SpeedUp);
    setScale("0.5"_fx);
    CHECK(firesWhen(h, up));
    Predicate down;
    down.kind    = PredKind::Motion;
    down.subKind = static_cast<int32_t>(MotionCheck::SpeedDown);
    setScale("-0.5"_fx);
    CHECK(firesWhen(h, down));
    Predicate air;
    air.kind    = PredKind::Motion;
    air.subKind = static_cast<int32_t>(MotionCheck::Air);
    CHECK_FALSE(firesWhen(h, air));
    h.world.ecs().get<TransformComponent>(h.self).position.y = Fixed::fromInt(10);
    CHECK(firesWhen(h, air));
    Predicate grounded;
    grounded.kind    = PredKind::Motion;
    grounded.subKind = static_cast<int32_t>(MotionCheck::NotAir);
    CHECK_FALSE(firesWhen(h, grounded));
    h.world.ecs().get<TransformComponent>(h.self).position.y = {};
    CHECK(firesWhen(h, grounded));
}

TEST_CASE("buff predicate Ratio")
{
    Harness h;
    h.world.ecs().get<HealthComponent>(h.self).hp = Fixed::fromInt(3000);
    Predicate pred;
    pred.kind          = PredKind::Ratio;
    pred.subKind       = static_cast<int32_t>(RatioRes::Hp);
    pred.hasMin        = true;
    pred.hasMax        = true;
    pred.minInclusive  = true;
    pred.maxInclusive  = true;
    pred.min           = "0.2"_fx;
    pred.max           = "0.4"_fx;
    CHECK(firesWhen(h, pred));
    pred.min = "0.5"_fx;
    pred.max = "0.6"_fx;
    CHECK_FALSE(firesWhen(h, pred));
}

TEST_CASE("buff predicate Delta")
{
    Harness h;
    auto& health = h.world.ecs().get<HealthComponent>(h.self);
    health.hp     = Fixed::fromInt(4000);
    health.prevHp = Fixed::fromInt(8000);
    health.ep     = Fixed::fromInt(100);
    health.prevEp = Fixed::fromInt(400);
    Predicate lost;
    lost.kind    = PredKind::Delta;
    lost.subKind = static_cast<int32_t>(DeltaCheck::LostHp);
    CHECK(firesWhen(h, lost));
    Predicate recovered;
    recovered.kind    = PredKind::Delta;
    recovered.subKind = static_cast<int32_t>(DeltaCheck::RecoveredHp);
    CHECK_FALSE(firesWhen(h, recovered));
    health.hp     = Fixed::fromInt(9000);
    health.prevHp = Fixed::fromInt(4000);
    CHECK(firesWhen(h, recovered));
    Predicate lostEp;
    lostEp.kind    = PredKind::Delta;
    lostEp.subKind = static_cast<int32_t>(DeltaCheck::LostEp);
    CHECK(firesWhen(h, lostEp));
    health.ep     = Fixed::fromInt(500);
    health.prevEp = Fixed::fromInt(100);
    Predicate recEp;
    recEp.kind    = PredKind::Delta;
    recEp.subKind = static_cast<int32_t>(DeltaCheck::RecoveredEp);
    CHECK(firesWhen(h, recEp));
    health.hp     = Fixed::fromInt(5000);
    health.prevHp = Fixed::fromInt(10000);
    Predicate pct;
    pct.kind    = PredKind::Delta;
    pct.subKind = static_cast<int32_t>(DeltaCheck::HpChangePercent);
    pct.min     = "-0.6"_fx;
    pct.max     = "-0.4"_fx;
    CHECK(firesWhen(h, pct));
}

TEST_CASE("buff predicate RoleType")
{
    Harness h;
    Predicate hero;
    hero.kind    = PredKind::RoleType;
    hero.subKind = static_cast<int32_t>(RoleCheck::Hero);
    CHECK(firesWhen(h, hero));
    Predicate monster;
    monster.kind    = PredKind::RoleType;
    monster.subKind = static_cast<int32_t>(RoleCheck::Monster);
    CHECK_FALSE(firesWhen(h, monster));
    h.world.ecs().get<IdentityComponent>(h.self).roleType = 4;
    CHECK(firesWhen(h, monster));
    Predicate eq;
    eq.kind    = PredKind::RoleType;
    eq.subKind = static_cast<int32_t>(RoleCheck::TypeEq);
    eq.id      = 4;
    CHECK(firesWhen(h, eq));
    Predicate sign;
    sign.kind    = PredKind::RoleType;
    sign.subKind = static_cast<int32_t>(RoleCheck::Sign);
    sign.ids     = {4, 5};
    CHECK(firesWhen(h, sign));
}

TEST_CASE("buff predicate Skill")
{
    Harness h;
    auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
    book.slots = {"A", "F", "Y"};
    SkillRuntime a;
    a.slot      = 0;
    a.configId  = 920000;
    a.step      = 1;
    a.stepCount = 3;
    SkillRuntime f;
    f.slot     = 1;
    f.configId = 11;
    f.timing   = true;
    f.cd       = Fixed::fromInt(100);
    book.skills = {a, f};
    h.world.ecs().get<SkillCastComponent>(h.self).skill = 0;
    h.world.ecs().get<ActionTimelineComponent>(h.self).clipId = 55;

    auto check = [&](SkillCheck kind, int32_t id = 0, std::vector<int32_t> ids = {}) {
        Predicate pred;
        pred.kind    = PredKind::Skill;
        pred.subKind = static_cast<int32_t>(kind);
        pred.id      = id;
        pred.ids     = std::move(ids);
        return firesWhen(h, pred);
    };

    CHECK(check(SkillCheck::CommandAngle));
    CHECK(check(SkillCheck::OpeningStep));
    CHECK(check(SkillCheck::ChainFirst));
    CHECK_FALSE(check(SkillCheck::ChainLast));
    CHECK(check(SkillCheck::ChainNotLast));
    CHECK(check(SkillCheck::DifferentSlot));
    CHECK(check(SkillCheck::SlotList, 0, {0}));
    CHECK(check(SkillCheck::JudgeTable, 3));
    CHECK(check(SkillCheck::SkillId, 920000));
    CHECK(check(SkillCheck::ActionId, 55));
    CHECK(check(SkillCheck::OnCooldown, 11));
    book.skills[0].finally = true;
    CHECK(check(SkillCheck::ChainLast));
}

TEST_CASE("buff predicate Artifact is inactive")
{
    Harness h;
    Predicate pred;
    pred.kind    = PredKind::Artifact;
    pred.subKind = static_cast<int32_t>(ArtifactCheck::Id);
    pred.id      = 1;
    CHECK_FALSE(firesWhen(h, pred));
}

TEST_CASE("buff predicate Event")
{
    Harness h;
    auto fireBegin = [&](Predicate pred, BuffContext ctx) {
        const Fixed before = h.world.ecs().get<HealthComponent>(h.self).hp;
        BuffDef def        = makeBuff(nextBuffId());
        def.lifecycle.executeType = 3;
        def.activation.beginOn    = TriggerPoint::AfterHit;
        BuffOp op                     = resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-1)));
        op.when                     = pred;
        def.onBegin.push_back(op);
        h.admit(h.add(def));
        h.fire(TriggerPoint::AfterHit, ctx);
        return h.world.ecs().get<HealthComponent>(h.self).hp < before;
    };

    Predicate crit;
    crit.kind    = PredKind::Event;
    crit.subKind = static_cast<int32_t>(EventCheck::Crit);
    BuffContext ctx;
    ctx.critical = true;
    CHECK(fireBegin(crit, ctx));
    ctx.critical = false;
    CHECK_FALSE(fireBegin(crit, ctx));

    Predicate dead;
    dead.kind    = PredKind::Event;
    dead.subKind = static_cast<int32_t>(EventCheck::TargetDead);
    ctx.targetDead = true;
    CHECK(fireBegin(dead, ctx));

    Predicate tp;
    tp.kind    = PredKind::Event;
    tp.subKind = static_cast<int32_t>(EventCheck::UseTp);
    ctx          = {};
    ctx.tpSpent = Fixed::fromInt(10);
    CHECK(fireBegin(tp, ctx));

    Predicate dark;
    dark.kind    = PredKind::Event;
    dark.subKind = static_cast<int32_t>(EventCheck::DarkHit);
    ctx            = {};
    ctx.effectId = 7;
    CHECK(fireBegin(dark, ctx));
}

TEST_CASE("buff predicate Timer")
{
    Harness h;
    auto& state = h.world.ecs().get<RoleStateComponent>(h.self);
    state.movedSec    = Fixed::fromInt(3);
    state.standingSec = Fixed::fromInt(1);
    Predicate moved;
    moved.kind    = PredKind::Timer;
    moved.subKind = static_cast<int32_t>(TimerCheck::Moved);
    moved.min     = Fixed::fromInt(2);
    CHECK(firesWhen(h, moved));
    Predicate standing;
    standing.kind    = PredKind::Timer;
    standing.subKind = static_cast<int32_t>(TimerCheck::Standing);
    standing.min     = Fixed::fromInt(2);
    CHECK_FALSE(firesWhen(h, standing));
    Predicate moving;
    moving.kind    = PredKind::Timer;
    moving.subKind = static_cast<int32_t>(TimerCheck::Moving);
    moving.min     = Fixed::fromInt(0);
    CHECK(firesWhen(h, moving));
}

TEST_CASE("buff predicate Misc")
{
    Harness h;
    Predicate mp;
    mp.kind    = PredKind::Misc;
    mp.subKind = static_cast<int32_t>(MiscCheck::MpEnough);
    mp.min     = Fixed::fromInt(50);
    CHECK(firesWhen(h, mp));
    h.world.ecs().get<HealthComponent>(h.self).mp = Fixed::fromInt(10);
    CHECK_FALSE(firesWhen(h, mp));

    Predicate crystal;
    crystal.kind    = PredKind::Misc;
    crystal.subKind = static_cast<int32_t>(MiscCheck::CrystalEnough);
    CHECK_FALSE(firesWhen(h, crystal));

    Predicate combo;
    combo.kind    = PredKind::Misc;
    combo.subKind = static_cast<int32_t>(MiscCheck::ComboEval);
    combo.id      = 2;
    CHECK_FALSE(firesWhen(h, combo));
    h.world.ecs().get<ComboMeterComponent>(h.self).grade = 3;
    CHECK(firesWhen(h, combo));

    Predicate rage;
    rage.kind    = PredKind::Misc;
    rage.subKind = static_cast<int32_t>(MiscCheck::RageTransform);
    CHECK_FALSE(firesWhen(h, rage));
    h.world.ecs().get<RageStateComponent>(h.self).transformed = true;
    CHECK(firesWhen(h, rage));

    Predicate never;
    never.kind    = PredKind::Misc;
    never.subKind = static_cast<int32_t>(MiscCheck::Never);
    CHECK_FALSE(firesWhen(h, never));
}

TEST_CASE("buff predicate EventTarget subject")
{
    Harness h;
    h.world.ecs().get<StatusFlagsComponent>(h.other).addFlag(BuffFlag::Stun);
    Predicate pred;
    pred.kind    = PredKind::HasFlag;
    pred.subject = Subject::EventTarget;
    pred.id      = static_cast<int32_t>(BuffFlag::Stun);
    BuffDef def    = makeBuff(nextBuffId());
    def.lifecycle.executeType = 3;
    def.activation.beginOn    = TriggerPoint::AfterHit;
    BuffOp op                     = resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-1)));
    op.when                     = pred;
    def.onBegin.push_back(op);
    const Fixed before = h.world.ecs().get<HealthComponent>(h.self).hp;
    h.admit(h.add(def));
    BuffContext ctx;
    ctx.target = h.other;
    h.fire(TriggerPoint::AfterHit, ctx);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp < before);
}
