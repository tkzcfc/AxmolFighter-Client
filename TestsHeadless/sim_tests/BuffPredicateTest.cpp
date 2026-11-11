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
    const Fixed before = h.world.ecs().get<Health>(h.self).m_hp;
    BuffDef def        = makeBuff(nextBuffId());
    BuffOp op          = resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-1)));
    op.m_when          = pred;
    def.m_onEnter.push_back(op);
    h.admit(h.add(def));
    return h.world.ecs().get<Health>(h.self).m_hp < before;
}

void addMarker(Harness& h, int32_t& id, int32_t ruleId, int32_t buffType)
{
    BuffDef mark                 = makeBuff(nextBuffId());
    mark.m_admission.m_ruleId    = ruleId;
    mark.m_admission.m_buffType  = buffType;
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
    pred.m_kind = PredKind::HasRule;
    pred.m_id   = 88001;
    CHECK(firesWhen(h, pred));
    Predicate miss;
    miss.m_kind = PredKind::HasRule;
    miss.m_id   = 88002;
    CHECK_FALSE(firesWhen(h, miss));
    Predicate all;
    all.m_kind  = PredKind::HasRule;
    all.m_allOf = true;
    all.m_ids   = {88001, 88002};
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
    pred.m_kind = PredKind::HasBuff;
    pred.m_id   = marker;
    CHECK(firesWhen(h, pred));
    Predicate miss;
    miss.m_kind      = PredKind::HasBuff;
    miss.m_id        = marker;
    miss.m_minStacks = 2;
    CHECK_FALSE(firesWhen(h, miss));
}

TEST_CASE("buff predicate HasBuffType")
{
    Harness h;
    int32_t marker = 0;
    addMarker(h, marker, 88200, 9);
    Predicate pred;
    pred.m_kind = PredKind::HasBuffType;
    pred.m_id   = 9;
    CHECK(firesWhen(h, pred));
    Predicate miss;
    miss.m_kind = PredKind::HasBuffType;
    miss.m_id   = 8;
    CHECK_FALSE(firesWhen(h, miss));
}

TEST_CASE("buff predicate HasFlag")
{
    Harness h;
    h.world.ecs().get<StatusFlags>(h.self).addFlag(BuffFlag::SuperArmor);
    Predicate pred;
    pred.m_kind = PredKind::HasFlag;
    pred.m_id   = static_cast<int32_t>(BuffFlag::SuperArmor);
    CHECK(firesWhen(h, pred));
    Predicate crazy;
    crazy.m_kind = PredKind::HasFlag;
    crazy.m_id   = static_cast<int32_t>(BuffFlag::Crazy);
    CHECK_FALSE(firesWhen(h, crazy));
    h.world.ecs().get<SkillCast>(h.self).m_crazy = true;
    CHECK(firesWhen(h, crazy));
    Predicate notCrazy;
    notCrazy.m_kind = PredKind::HasFlag;
    notCrazy.m_id   = static_cast<int32_t>(BuffFlag::NotCrazy);
    CHECK_FALSE(firesWhen(h, notCrazy));
}

TEST_CASE("buff predicate Motion")
{
    Harness h;
    auto setScale = [&](Fixed delta) {
        h.world.ecs().get<Stats>(h.self).m_mods.clear();
        h.world.ecs().get<Stats>(h.self).m_mods.push_back({200, delta, 0});
    };
    Predicate up;
    up.m_kind    = PredKind::Motion;
    up.m_subKind = static_cast<int32_t>(MotionCheck::SpeedUp);
    setScale("0.5"_fx);
    CHECK(firesWhen(h, up));
    Predicate down;
    down.m_kind    = PredKind::Motion;
    down.m_subKind = static_cast<int32_t>(MotionCheck::SpeedDown);
    setScale("-0.5"_fx);
    CHECK(firesWhen(h, down));
    Predicate air;
    air.m_kind    = PredKind::Motion;
    air.m_subKind = static_cast<int32_t>(MotionCheck::Air);
    CHECK_FALSE(firesWhen(h, air));
    h.world.ecs().get<Transform>(h.self).m_position.y = Fixed::fromInt(10);
    CHECK(firesWhen(h, air));
    Predicate grounded;
    grounded.m_kind    = PredKind::Motion;
    grounded.m_subKind = static_cast<int32_t>(MotionCheck::NotAir);
    CHECK_FALSE(firesWhen(h, grounded));
    h.world.ecs().get<Transform>(h.self).m_position.y = {};
    CHECK(firesWhen(h, grounded));
}

TEST_CASE("buff predicate Ratio")
{
    Harness h;
    h.world.ecs().get<Health>(h.self).m_hp = Fixed::fromInt(3000);
    Predicate pred;
    pred.m_kind          = PredKind::Ratio;
    pred.m_subKind       = static_cast<int32_t>(RatioRes::Hp);
    pred.m_hasMin        = true;
    pred.m_hasMax        = true;
    pred.m_minInclusive  = true;
    pred.m_maxInclusive  = true;
    pred.m_min           = "0.2"_fx;
    pred.m_max           = "0.4"_fx;
    CHECK(firesWhen(h, pred));
    pred.m_min = "0.5"_fx;
    pred.m_max = "0.6"_fx;
    CHECK_FALSE(firesWhen(h, pred));
}

TEST_CASE("buff predicate Delta")
{
    Harness h;
    auto& health = h.world.ecs().get<Health>(h.self);
    health.m_hp     = Fixed::fromInt(4000);
    health.m_prevHp = Fixed::fromInt(8000);
    health.m_ep     = Fixed::fromInt(100);
    health.m_prevEp = Fixed::fromInt(400);
    Predicate lost;
    lost.m_kind    = PredKind::Delta;
    lost.m_subKind = static_cast<int32_t>(DeltaCheck::LostHp);
    CHECK(firesWhen(h, lost));
    Predicate recovered;
    recovered.m_kind    = PredKind::Delta;
    recovered.m_subKind = static_cast<int32_t>(DeltaCheck::RecoveredHp);
    CHECK_FALSE(firesWhen(h, recovered));
    health.m_hp     = Fixed::fromInt(9000);
    health.m_prevHp = Fixed::fromInt(4000);
    CHECK(firesWhen(h, recovered));
    Predicate lostEp;
    lostEp.m_kind    = PredKind::Delta;
    lostEp.m_subKind = static_cast<int32_t>(DeltaCheck::LostEp);
    CHECK(firesWhen(h, lostEp));
    health.m_ep     = Fixed::fromInt(500);
    health.m_prevEp = Fixed::fromInt(100);
    Predicate recEp;
    recEp.m_kind    = PredKind::Delta;
    recEp.m_subKind = static_cast<int32_t>(DeltaCheck::RecoveredEp);
    CHECK(firesWhen(h, recEp));
    health.m_hp     = Fixed::fromInt(5000);
    health.m_prevHp = Fixed::fromInt(10000);
    Predicate pct;
    pct.m_kind    = PredKind::Delta;
    pct.m_subKind = static_cast<int32_t>(DeltaCheck::HpChangePercent);
    pct.m_min     = "-0.6"_fx;
    pct.m_max     = "-0.4"_fx;
    CHECK(firesWhen(h, pct));
}

TEST_CASE("buff predicate RoleType")
{
    Harness h;
    Predicate hero;
    hero.m_kind    = PredKind::RoleType;
    hero.m_subKind = static_cast<int32_t>(RoleCheck::Hero);
    CHECK(firesWhen(h, hero));
    Predicate monster;
    monster.m_kind    = PredKind::RoleType;
    monster.m_subKind = static_cast<int32_t>(RoleCheck::Monster);
    CHECK_FALSE(firesWhen(h, monster));
    h.world.ecs().get<Identity>(h.self).m_roleType = 4;
    CHECK(firesWhen(h, monster));
    Predicate eq;
    eq.m_kind    = PredKind::RoleType;
    eq.m_subKind = static_cast<int32_t>(RoleCheck::TypeEq);
    eq.m_id      = 4;
    CHECK(firesWhen(h, eq));
    Predicate sign;
    sign.m_kind    = PredKind::RoleType;
    sign.m_subKind = static_cast<int32_t>(RoleCheck::Sign);
    sign.m_ids     = {4, 5};
    CHECK(firesWhen(h, sign));
}

TEST_CASE("buff predicate Skill")
{
    Harness h;
    auto& book = h.world.ecs().get<SkillBook>(h.self);
    book.m_slots = {"A", "F", "Y"};
    SkillRuntime a;
    a.m_slot      = 0;
    a.m_configId  = 920000;
    a.m_step      = 1;
    a.m_stepCount = 3;
    SkillRuntime f;
    f.m_slot     = 1;
    f.m_configId = 11;
    f.m_timing   = true;
    f.m_cd       = Fixed::fromInt(100);
    book.m_skills = {a, f};
    h.world.ecs().get<SkillCast>(h.self).m_skill = 0;
    h.world.ecs().get<ActionTimeline>(h.self).m_clipId = 55;

    auto check = [&](SkillCheck kind, int32_t id = 0, std::vector<int32_t> ids = {}) {
        Predicate pred;
        pred.m_kind    = PredKind::Skill;
        pred.m_subKind = static_cast<int32_t>(kind);
        pred.m_id      = id;
        pred.m_ids     = std::move(ids);
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
    book.m_skills[0].m_finally = true;
    CHECK(check(SkillCheck::ChainLast));
}

TEST_CASE("buff predicate Artifact is inactive")
{
    Harness h;
    Predicate pred;
    pred.m_kind    = PredKind::Artifact;
    pred.m_subKind = static_cast<int32_t>(ArtifactCheck::Id);
    pred.m_id      = 1;
    CHECK_FALSE(firesWhen(h, pred));
}

TEST_CASE("buff predicate Event")
{
    Harness h;
    auto fireBegin = [&](Predicate pred, BuffContext ctx) {
        const Fixed before = h.world.ecs().get<Health>(h.self).m_hp;
        BuffDef def        = makeBuff(nextBuffId());
        def.m_lifecycle.m_executeType = 3;
        def.m_activation.m_beginOn    = TriggerPoint::AfterHit;
        BuffOp op                     = resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-1)));
        op.m_when                     = pred;
        def.m_onBegin.push_back(op);
        h.admit(h.add(def));
        h.fire(TriggerPoint::AfterHit, ctx);
        return h.world.ecs().get<Health>(h.self).m_hp < before;
    };

    Predicate crit;
    crit.m_kind    = PredKind::Event;
    crit.m_subKind = static_cast<int32_t>(EventCheck::Crit);
    BuffContext ctx;
    ctx.m_critical = true;
    CHECK(fireBegin(crit, ctx));
    ctx.m_critical = false;
    CHECK_FALSE(fireBegin(crit, ctx));

    Predicate dead;
    dead.m_kind    = PredKind::Event;
    dead.m_subKind = static_cast<int32_t>(EventCheck::TargetDead);
    ctx.m_targetDead = true;
    CHECK(fireBegin(dead, ctx));

    Predicate tp;
    tp.m_kind    = PredKind::Event;
    tp.m_subKind = static_cast<int32_t>(EventCheck::UseTp);
    ctx          = {};
    ctx.m_tpSpent = Fixed::fromInt(10);
    CHECK(fireBegin(tp, ctx));

    Predicate dark;
    dark.m_kind    = PredKind::Event;
    dark.m_subKind = static_cast<int32_t>(EventCheck::DarkHit);
    ctx            = {};
    ctx.m_effectId = 7;
    CHECK(fireBegin(dark, ctx));
}

TEST_CASE("buff predicate Timer")
{
    Harness h;
    auto& state = h.world.ecs().get<RoleState>(h.self);
    state.m_movedSec    = Fixed::fromInt(3);
    state.m_standingSec = Fixed::fromInt(1);
    Predicate moved;
    moved.m_kind    = PredKind::Timer;
    moved.m_subKind = static_cast<int32_t>(TimerCheck::Moved);
    moved.m_min     = Fixed::fromInt(2);
    CHECK(firesWhen(h, moved));
    Predicate standing;
    standing.m_kind    = PredKind::Timer;
    standing.m_subKind = static_cast<int32_t>(TimerCheck::Standing);
    standing.m_min     = Fixed::fromInt(2);
    CHECK_FALSE(firesWhen(h, standing));
    Predicate moving;
    moving.m_kind    = PredKind::Timer;
    moving.m_subKind = static_cast<int32_t>(TimerCheck::Moving);
    moving.m_min     = Fixed::fromInt(0);
    CHECK(firesWhen(h, moving));
}

TEST_CASE("buff predicate Misc")
{
    Harness h;
    Predicate mp;
    mp.m_kind    = PredKind::Misc;
    mp.m_subKind = static_cast<int32_t>(MiscCheck::MpEnough);
    mp.m_min     = Fixed::fromInt(50);
    CHECK(firesWhen(h, mp));
    h.world.ecs().get<Health>(h.self).m_mp = Fixed::fromInt(10);
    CHECK_FALSE(firesWhen(h, mp));

    Predicate crystal;
    crystal.m_kind    = PredKind::Misc;
    crystal.m_subKind = static_cast<int32_t>(MiscCheck::CrystalEnough);
    CHECK_FALSE(firesWhen(h, crystal));

    Predicate combo;
    combo.m_kind    = PredKind::Misc;
    combo.m_subKind = static_cast<int32_t>(MiscCheck::ComboEval);
    combo.m_id      = 2;
    CHECK_FALSE(firesWhen(h, combo));
    h.world.ecs().get<ComboMeter>(h.self).m_grade = 3;
    CHECK(firesWhen(h, combo));

    Predicate rage;
    rage.m_kind    = PredKind::Misc;
    rage.m_subKind = static_cast<int32_t>(MiscCheck::RageTransform);
    CHECK_FALSE(firesWhen(h, rage));
    h.world.ecs().get<RageState>(h.self).m_transformed = true;
    CHECK(firesWhen(h, rage));

    Predicate never;
    never.m_kind    = PredKind::Misc;
    never.m_subKind = static_cast<int32_t>(MiscCheck::Never);
    CHECK_FALSE(firesWhen(h, never));
}

TEST_CASE("buff predicate EventTarget subject")
{
    Harness h;
    h.world.ecs().get<StatusFlags>(h.other).addFlag(BuffFlag::Stun);
    Predicate pred;
    pred.m_kind    = PredKind::HasFlag;
    pred.m_subject = Subject::EventTarget;
    pred.m_id      = static_cast<int32_t>(BuffFlag::Stun);
    BuffDef def    = makeBuff(nextBuffId());
    def.m_lifecycle.m_executeType = 3;
    def.m_activation.m_beginOn    = TriggerPoint::AfterHit;
    BuffOp op                     = resourceOp(ResourceId::Hp, constVal(Fixed::fromInt(-1)));
    op.m_when                     = pred;
    def.m_onBegin.push_back(op);
    const Fixed before = h.world.ecs().get<Health>(h.self).m_hp;
    h.admit(h.add(def));
    BuffContext ctx;
    ctx.m_target = h.other;
    h.fire(TriggerPoint::AfterHit, ctx);
    CHECK(h.world.ecs().get<Health>(h.self).m_hp < before);
}
