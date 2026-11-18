#include "BuffTestSupport.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace buff_test;
using namespace mugen;
using namespace mugen::sim;
using namespace mugen::conf::battle;

namespace
{
// DESIGN §5.8 未移植规则类（旧分派表 isUnportedRule）。
struct UnportedRule
{
    int32_t id = 0;
    const char* name = nullptr;
};

constexpr UnportedRule kUnported[] = {
    {15, "BuffThunder3"},
    {16, "BuffPlague1"},
    {17, "BuffPlague2"},
    {18, "BuffPlague3"},
    {20, "BuffMagicUp"},
    {21, "BuffDark2"},
    {24, "BuffPawMark"},
    {27, "BuffPassivation"},
    {32, "BuffBurn2"},
    {34, "BuffFlaw"},
    {37, "BuffGuard"},
    {38, "BuffBloodBeast"},
    {40, "BuffRedPython"},
    {41, "BuffMeltingArmor"},
    {50, "BuffLightWing"},
    {104, "BuffAddBySelect"},
    {401, "BuffRemoveByTypeAddExtra"},
    {600, "BuffAddByComboEvaluate"},
    {800, "BuffAttributeCombo"},
    {900, "BuffAttributeSlotCD"},
    {1002, "BuffDamageHurt3"},
    {1011, "BuffDamageCritDef"},
    {1051, "BuffDamageCritHPLoss"},
    {1070, "BuffDamageTPLoss"},
    {1071, "BuffCurtTPLoss"},
    {1080, "BuffDamageTPResidue"},
    {1091, "BuffHurtEPPercent"},
    {1101, "BuffHPTypeGradation"},
    {1102, "BuffCritTypeGradation"},
    {1103, "BuffCritDamageTypeGradation"},
    {1200, "BuffDefendHPLoss"},
    {1210, "BuffDamageBySpeed"},
    {1220, "BuffDamageCritByHPLost"},
    {1230, "BuffFloorTm"},
    {2002, "BuffModifyHPBasedByLostHp"},
    {2010, "BuffHPMAX"},
    {2020, "BuffHealingChange"},
    {3030, "BuffCPConsumeScale"},
    {3040, "BuffEPPlus"},
    {4001, "BuffTpAddNewBuff"},
    {4031, "BuffTPRecoveryScaleBySpeed"},
    {4032, "BuffTPRecoveryScaleBySlotCD"},
    {5001, "BuffUpdateArtifactCD"},
    {5020, "BuffHemophagia"},
    {5030, "BuffModifyCDSkill"},
    {5040, "BuffConsumeCrystal"},
    {5060, "BuffCritHPLoss"},
    {5080, "BuffDeductEpReplaceHp"},
    {5090, "BuffDeductTpReplaceHp"},
    {5100, "BuffImmune"},
    {5120, "BuffExtendDebuff"},
    {5130, "BuffNotConsumeCrystal"},
    {7001, "BuffWithTypeAddBuff"},
    {7003, "BuffWithTypeAddBuff2"},
    {9000, "BuffResetSkillCD"},
    {9001, "BuffResetArtifactCD"},
    {9002, "BuffModifyCDSkillNew"},
    {10003, "BuffCDArtifact"},
    {20002, "BuffDisableArt"},
    {20003, "BuffDisableSlots"},
    {21001, "BuffChangeRoleAcitonScaleTime"},
    {21002, "BuffSealSkill"},
    {21003, "BuffSummonDummy"},
    {21004, "BuffDummyDeathRattle"},
};

static_assert(sizeof(kUnported) / sizeof(kUnported[0]) == 64);

const std::string& lowerReport()
{
    static const std::string text = [] {
        const std::string path = std::string(MUGEN_CONTENT_DIR) + "/mugen/config/lower_report.txt";
        std::ifstream in(path);
        REQUIRE(in.good());
        return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }();
    return text;
}

bool reportLists(const char* name)
{
    return lowerReport().find(name) != std::string::npos;
}

bool areaOkDefault(const BuffDef& row)
{
    const auto& mask = row.admission.areaMask;
    if (mask.empty() || mask.front() == -1)
        return true;
    bool listed = false;
    for (size_t i = 1; i < mask.size(); ++i)
        if (mask[i] == 0)
            listed = true;
    if (mask.front() == 0)
        return !listed;
    if (mask.front() == 1)
        return listed;
    return true;
}

const BuffDef* findRule(int32_t ruleId)
{
    const BuffDef* best = nullptr;
    int32_t bestScore   = -1;
    for (const auto& row : conf::Config::instance().table<BuffDef>())
    {
        if (row.admission.ruleId != ruleId)
            continue;
        int32_t score = 0;
        if (row.status != LowerStatus::Unsupported)
            score += 8;
        if (row.activation.conditions.empty())
            score += 16;
        if (row.lifecycle.executeType == 1)
            score += 4;
        if (row.lifecycle.executeType == 3 && row.activation.beginOn != TriggerPoint::None)
            score += 6;
        if (row.admission.probability >= Fixed::fromInt(100))
            score += 2;
        if (areaOkDefault(row))
            score += 12;
        if (!row.activation.oneShot)
            score += 2;
        else
            score -= 4;
        const int32_t enter = static_cast<int32_t>(BuffHook::Enter);
        const int32_t begin = static_cast<int32_t>(BuffHook::Begin);
        if (!row.onEnter.empty() || (row.activation.latchOn & enter) != 0)
            score += 4;
        if (row.activation.beginOn != TriggerPoint::None &&
            ((row.activation.latchOn & begin) != 0 || !row.onBegin.empty()))
            score += 6;
        if (row.activation.beginOn == TriggerPoint::None && row.onEnter.empty() &&
            (row.activation.latchOn & begin) != 0 && (row.activation.latchOn & enter) == 0)
            score -= 8;
        if (row.lifecycle.executeType == 2 && row.lifecycle.times == 1 &&
            row.lifecycle.intervalSec == Fixed{})
            score -= 4;
        for (const auto& op : row.persistent)
            if (op.effect == OpEffect::Active &&
                (op.kind == BuffOpKind::Stat || op.kind == BuffOpKind::HpLock) && op.value.a != Fixed{})
                score += 3;
        if (score > bestScore)
        {
            bestScore = score;
            best      = &row;
        }
    }
    return best;
}

template <class Fn>
void eachOp(const BuffDef& def, Fn&& fn)
{
    for (const auto& op : def.persistent)
        fn(op);
    for (const auto& op : def.onEnter)
        fn(op);
    for (const auto& op : def.onTick)
        fn(op);
    for (const auto& op : def.onBegin)
        fn(op);
    for (const auto& op : def.onEnd)
        fn(op);
    for (const auto& op : def.onExit)
        fn(op);
    for (const auto& op : def.watch)
        fn(op);
    for (const auto& op : def.choicePool)
        fn(op);
}

bool hasActiveKind(const BuffDef& def, BuffOpKind kind)
{
    bool found = false;
    eachOp(def, [&](const BuffOp& op) {
        if (op.kind == kind && op.effect == OpEffect::Active)
            found = true;
    });
    return found;
}

const BuffOp* firstKind(const std::vector<BuffOp>& ops, BuffOpKind kind)
{
    for (const auto& op : ops)
        if (op.kind == kind)
            return &op;
    return nullptr;
}

void seedBaseSlots(Harness& h, int32_t cooling = 1)
{
    auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
    book.slots = {"A", "B", "C", "D", "E", "F", "Y"};
    book.skills.clear();
    for (int32_t i = 0; i < 7; ++i)
    {
        SkillRuntime skill;
        skill.slot        = i;
        skill.configId    = 920000;
        skill.cd          = Fixed::fromInt(4000);
        skill.coldMaxTime = Fixed::fromInt(8000);
        skill.timing      = i < cooling;
        book.skills.push_back(skill);
    }
}

void prepareValue(Harness& h, const ValueExpr& expr)
{
    if (expr.kind == ValueKind::Lost)
    {
        auto& health = h.world.ecs().get<HealthComponent>(h.self);
        if (expr.res == ResourceId::Hp)
            health.hp = health.maxHp / Fixed::fromInt(2);
        else if (expr.res == ResourceId::Mp)
            health.mp = health.maxMp / Fixed::fromInt(2);
        else if (expr.res == ResourceId::Ep)
            health.ep = health.maxEp / Fixed::fromInt(2);
    }
    else if (expr.kind == ValueKind::SpeedBonus)
    {
        h.world.ecs().get<StatsComponent>(h.self).mods.push_back(
            {static_cast<int32_t>(BuffStat::MoveScaleX), "0.5"_fx, 0, false});
    }
    else if (expr.kind == ValueKind::ComboTable)
    {
        auto& combo = h.world.ecs().get<ComboMeterComponent>(h.self);
        combo.grade = 0;
        for (int32_t i = static_cast<int32_t>(expr.table.size()) - 1; i >= 0; --i)
            if (expr.table[static_cast<size_t>(i)] != Fixed{})
            {
                combo.grade = i;
                break;
            }
    }
    else if (expr.kind == ValueKind::CoolingSlots || expr.kind == ValueKind::CoolingSlotTable)
        seedBaseSlots(h, 1);
    else if (expr.kind == ValueKind::TypeCount || expr.kind == ValueKind::MaxTimes)
    {
        for (const int32_t type : expr.types)
        {
            BuffDef mark                = makeBuff(nextBuffId());
            mark.admission.buffType = type;
            h.admit(h.add(mark));
        }
    }
}

void prepareDef(Harness& h, const BuffDef& def)
{
    eachOp(def, [&](const BuffOp& op) { prepareValue(h, op.value); });
}

BuffDef* mutableRule(int32_t id)
{
    for (auto& row : buffTable())
        if (row.id == id)
            return &row;
    return nullptr;
}

void armBegin(BuffDef& def)
{
    if (def.activation.beginOn != TriggerPoint::None)
        return;
    const int32_t enter = static_cast<int32_t>(BuffHook::Enter);
    if (def.lifecycle.executeType == 1 &&
        (!def.onEnter.empty() || (def.activation.latchOn & enter) != 0))
        return;
    const bool beginLatch = (def.activation.latchOn & static_cast<int32_t>(BuffHook::Begin)) != 0;
    if (beginLatch || !def.onBegin.empty())
        def.activation.beginOn = TriggerPoint::AfterHit;
}

void satisfy(Harness& h, const BuffDef& def, BuffContext& ctx)
{
    bool needSkill = false;
    auto applyPred = [&](const Predicate& pred) {
        if (pred.kind == PredKind::Skill)
        {
            needSkill = true;
            if (pred.subKind == static_cast<int32_t>(SkillCheck::ActionId))
            {
                auto& timeline = h.world.ecs().get<ActionTimelineComponent>(h.self);
                timeline.clipId = pred.id != 0 ? pred.id : (pred.ids.empty() ? 0 : pred.ids.front());
            }
        }
        if (pred.kind == PredKind::Event)
        {
            if (pred.subKind == static_cast<int32_t>(EventCheck::Crit))
                ctx.critical = true;
            if (pred.subKind == static_cast<int32_t>(EventCheck::UseTp))
                ctx.tpSpent = pred.min > Fixed{} ? pred.min : Fixed::fromInt(20);
        }
    };
    for (const auto& pred : def.activation.conditions)
        applyPred(pred);
    if (needSkill)
    {
        if (h.world.ecs().get<SkillBookComponent>(h.self).skills.empty())
            seedBaseSlots(h, 1);
        h.world.ecs().get<SkillCastComponent>(h.self).skill = 0;
    }
    if (def.activation.eventFilter == 2)
        ctx.target = h.other;
}

void activate(Harness& h, int32_t id, const BuffContext& ctx = {})
{
    auto* live            = mutableRule(id);
    TriggerPoint saved    = TriggerPoint::None;
    if (live)
    {
        saved = live->activation.beginOn;
        armBegin(*live);
    }
    const auto* def = conf::Config::instance().find<BuffDef>(id);
    BuffContext use = ctx;
    if (def)
        satisfy(h, *def, use);
    h.admit(id, use);
    def = conf::Config::instance().find<BuffDef>(id);
    if (def && def->activation.beginOn != TriggerPoint::None)
        h.fire(def->activation.beginOn, use);
    if (live)
        live->activation.beginOn = saved;
}

bool anyFlag(World& world, Entity entity)
{
    const auto& flags = world.ecs().get<StatusFlagsComponent>(entity);
    for (int32_t i = 1; i <= 8; ++i)
        if (flags.active(static_cast<ExtraState>(i)))
            return true;
    return flags.noHeal > 0 || flags.noCrystal > 0;
}

int32_t coolingCount(World& world, Entity entity)
{
    int32_t n        = 0;
    const auto& book = world.ecs().get<SkillBookComponent>(entity);
    for (const auto& skill : book.skills)
        if (skill.timing && skill.cd > Fixed{})
            ++n;
    return n;
}

void checkMarker(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    CHECK(def->status == LowerStatus::Lowered);
    CHECK(def->persistent.empty());
    CHECK_FALSE(hasActiveKind(*def, BuffOpKind::Dot));
    CHECK_FALSE(hasActiveKind(*def, BuffOpKind::Stat));
    const Fixed hp    = h.world.ecs().get<HealthComponent>(h.self).hp;
    const auto mods   = h.world.ecs().get<StatsComponent>(h.self).mods.size();
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp);
    CHECK(h.world.ecs().get<StatsComponent>(h.self).mods.size() == mods);
    CHECK_FALSE(anyFlag(h.world, h.self));
    CHECK(reportLists(name));
}

void checkDot(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    const auto* dot = firstKind(def->onEnter, BuffOpKind::Dot);
    REQUIRE(dot != nullptr);
    CHECK(dot->effect == OpEffect::Active);
    const Fixed hp = h.world.ecs().get<HealthComponent>(h.self).hp;
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    if (def->lifecycle.executeType == 2)
    {
        h.setTickSeconds(1);
        h.tickBuff(3);
    }
    const Fixed after = h.world.ecs().get<HealthComponent>(h.self).hp;
    if (dot->value.a < Fixed{})
        CHECK(after < hp);
    else if (dot->value.a > Fixed{})
        CHECK(after > hp);
    CHECK(reportLists(name));
}

void checkStatLatch(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    REQUIRE_FALSE(def->persistent.empty());
    prepareDef(h, *def);
    seedBaseSlots(h, 1);
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    int32_t checked = 0;
    for (const auto& op : def->persistent)
    {
        if (op.effect != OpEffect::Active)
            continue;
        if (op.kind == BuffOpKind::Stat)
        {
            CHECK(statSum(h.world, h.self, static_cast<int32_t>(op.stat)) != Fixed{});
            ++checked;
        }
        else if (op.kind == BuffOpKind::SkillMod)
        {
            CHECK_FALSE(h.world.ecs().get<SkillBookComponent>(h.self).mods.empty());
            ++checked;
        }
        else if (op.kind == BuffOpKind::Flag)
        {
            if (op.flag == BuffFlag::NoHeal)
                CHECK(h.world.ecs().get<StatusFlagsComponent>(h.self).noHeal > 0);
            else if (op.flag == BuffFlag::NoCrystalCost)
                CHECK(h.world.ecs().get<StatusFlagsComponent>(h.self).noCrystal > 0);
            else
                CHECK(hasFlag(h.world, h.self, static_cast<ExtraState>(op.flag)));
            ++checked;
        }
        else if (op.kind == BuffOpKind::Immune)
        {
            CHECK_FALSE(h.world.ecs().get<BuffListComponent>(h.self).immunity.empty());
            ++checked;
        }
        else if (op.kind == BuffOpKind::HpLock)
        {
            CHECK(statSum(h.world, h.self, kHpLockStat) != Fixed{});
            ++checked;
        }
    }
    CHECK(checked > 0);
    h.remove(def->id);
    CHECK(findBuff(h.world, h.self, def->id) == nullptr);
    for (const auto& op : def->persistent)
    {
        if (op.effect != OpEffect::Active || op.kind != BuffOpKind::Stat)
            continue;
        CHECK(statSum(h.world, h.self, static_cast<int32_t>(op.stat)) == Fixed{});
    }
    CHECK(reportLists(name));
}

void checkNoop(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    CHECK(def->status == LowerStatus::Noop);
    CHECK(reportLists(name));
    const Fixed hp    = h.world.ecs().get<HealthComponent>(h.self).hp;
    const auto mods   = h.world.ecs().get<StatsComponent>(h.self).mods.size();
    const auto skills = h.world.ecs().get<SkillBookComponent>(h.self).mods.size();
    h.admit(def->id);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp);
    CHECK(h.world.ecs().get<StatsComponent>(h.self).mods.size() == mods);
    CHECK(h.world.ecs().get<SkillBookComponent>(h.self).mods.size() == skills);
    CHECK_FALSE(anyFlag(h.world, h.self));
}

void checkUnsupported(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    CHECK(def->status == LowerStatus::Unsupported);
    CHECK(reportLists(name));
    CHECK(lowerReport().find("unsupported") != std::string::npos);
    const Fixed hp = h.world.ecs().get<HealthComponent>(h.self).hp;
    h.admit(def->id);
    CHECK(findBuff(h.world, h.self, def->id) == nullptr);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp);
    CHECK_FALSE(anyFlag(h.world, h.self));
}
}  // namespace

TEST_CASE("unported rule catalog is 64 and present in config.bin")
{
    buff_test::loadAssets();
    CHECK(sizeof(kUnported) / sizeof(kUnported[0]) == 64);
    for (const auto& rule : kUnported)
    {
        CAPTURE(rule.id);
        CAPTURE(rule.name);
        const auto* def = findRule(rule.id);
        REQUIRE(def != nullptr);
        CHECK(def->admission.ruleId == rule.id);
        CHECK(reportLists(rule.name));
    }
}

TEST_CASE("buff rule BuffThunder3 marker") { checkMarker(15, "BuffThunder3"); }
TEST_CASE("buff rule BuffPlague1 dot") { checkDot(16, "BuffPlague1"); }
TEST_CASE("buff rule BuffPlague2 dot") { checkDot(17, "BuffPlague2"); }
TEST_CASE("buff rule BuffPlague3 dot") { checkDot(18, "BuffPlague3"); }
TEST_CASE("buff rule BuffMagicUp AddHurt and MpCostScale") { checkStatLatch(20, "BuffMagicUp"); }
TEST_CASE("buff rule BuffDark2 marker") { checkMarker(21, "BuffDark2"); }
TEST_CASE("buff rule BuffPawMark marker") { checkMarker(24, "BuffPawMark"); }
TEST_CASE("buff rule BuffPassivation AvoidCrit") { checkStatLatch(27, "BuffPassivation"); }
TEST_CASE("buff rule BuffBurn2 dot") { checkDot(32, "BuffBurn2"); }
TEST_CASE("buff rule BuffFlaw AvoidHurt") { checkStatLatch(34, "BuffFlaw"); }
TEST_CASE("buff rule BuffGuard marker") { checkMarker(37, "BuffGuard"); }
TEST_CASE("buff rule BuffBloodBeast AvoidHurt and AddHurt") { checkStatLatch(38, "BuffBloodBeast"); }
TEST_CASE("buff rule BuffRedPython marker") { checkMarker(40, "BuffRedPython"); }
TEST_CASE("buff rule BuffMeltingArmor dot") { checkDot(41, "BuffMeltingArmor"); }
TEST_CASE("buff rule BuffLightWing marker") { checkMarker(50, "BuffLightWing"); }

TEST_CASE("buff rule BuffAddBySelect weighted Apply")
{
    Harness h;
    const auto* def = findRule(104);
    REQUIRE(def != nullptr);
    const auto* apply = firstKind(def->onEnter, BuffOpKind::Apply);
    REQUIRE(apply != nullptr);
    CHECK(apply->pick == PickMode::Weighted);
    REQUIRE_FALSE(apply->ids.empty());
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    bool child = false;
    for (const int32_t id : apply->ids)
        if (findBuff(h.world, h.self, id) != nullptr)
            child = true;
    CHECK(child);
}

TEST_CASE("buff rule BuffRemoveByTypeAddExtra counts then mods")
{
    Harness h;
    const auto* def = findRule(401);
    REQUIRE(def != nullptr);
    const auto* remove = firstKind(def->onEnter, BuffOpKind::Remove);
    REQUIRE(remove != nullptr);
    REQUIRE_FALSE(remove->ids.empty());
    BuffDef mark                = makeBuff(nextBuffId());
    mark.admission.buffType = remove->ids.front();
    const int32_t markId        = h.add(mark);
    h.admit(markId);
    REQUIRE(findBuff(h.world, h.self, markId) != nullptr);
    auto& health = h.world.ecs().get<HealthComponent>(h.self);
    health.mp  = Fixed::fromInt(10);
    activate(h, def->id);
    CHECK(findBuff(h.world, h.self, markId) == nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AddHurt)) != Fixed{});
}

TEST_CASE("buff rule BuffAddByComboEvaluate watches kids")
{
    Harness h;
    const auto* def = findRule(600);
    REQUIRE(def != nullptr);
    REQUIRE_FALSE(def->watch.empty());
    const auto& watch = def->watch.front();
    CHECK(watch.kind == BuffOpKind::Watch);
    CHECK(watch.watch == WatchMode::Latched);
    REQUIRE_FALSE(watch.ids.empty());
    const int32_t kid = watch.ids.front();
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    // 先满足条件合上门闩（bits=1、不挂子），再落到未满足以挂子。
    h.world.ecs().get<ComboMeterComponent>(h.self).grade = 99;
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, kid) == nullptr);
    h.world.ecs().get<ComboMeterComponent>(h.self).grade = 0;
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, kid) != nullptr);
    h.world.ecs().get<ComboMeterComponent>(h.self).grade = 99;
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, kid) == nullptr);
}

TEST_CASE("buff rule BuffAttributeCombo ComboTable") { checkStatLatch(800, "BuffAttributeCombo"); }
TEST_CASE("buff rule BuffAttributeSlotCD CoolingSlotTable") { checkStatLatch(900, "BuffAttributeSlotCD"); }
TEST_CASE("buff rule BuffDamageHurt3 noop artifact") { checkNoop(1002, "BuffDamageHurt3"); }
TEST_CASE("buff rule BuffDamageCritDef AddCritHurtDef") { checkStatLatch(1011, "BuffDamageCritDef"); }
TEST_CASE("buff rule BuffDamageCritHPLoss Lost Hp") { checkStatLatch(1051, "BuffDamageCritHPLoss"); }
TEST_CASE("buff rule BuffDamageTPLoss Lost Mp") { checkStatLatch(1070, "BuffDamageTPLoss"); }
TEST_CASE("buff rule BuffCurtTPLoss Lost Mp") { checkStatLatch(1071, "BuffCurtTPLoss"); }
TEST_CASE("buff rule BuffDamageTPResidue Cur Mp") { checkStatLatch(1080, "BuffDamageTPResidue"); }
TEST_CASE("buff rule BuffHurtEPPercent Cur Ep") { checkStatLatch(1091, "BuffHurtEPPercent"); }

TEST_CASE("buff rule BuffHPTypeGradation MaxTimes heal")
{
    Harness h;
    const auto* def = findRule(1101);
    REQUIRE(def != nullptr);
    prepareDef(h, *def);
    auto& health = h.world.ecs().get<HealthComponent>(h.self);
    health.hp  = health.maxHp / Fixed::fromInt(2);
    const Fixed hp = health.hp;
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp != hp);
}

TEST_CASE("buff rule BuffCritTypeGradation TypeCount") { checkStatLatch(1102, "BuffCritTypeGradation"); }
TEST_CASE("buff rule BuffCritDamageTypeGradation TypeCount") { checkStatLatch(1103, "BuffCritDamageTypeGradation"); }
TEST_CASE("buff rule BuffDefendHPLoss Lost Hp") { checkStatLatch(1200, "BuffDefendHPLoss"); }
TEST_CASE("buff rule BuffDamageBySpeed SpeedBonus") { checkStatLatch(1210, "BuffDamageBySpeed"); }
TEST_CASE("buff rule BuffDamageCritByHPLost constant AddCritHurt") { checkStatLatch(1220, "BuffDamageCritByHPLost"); }
TEST_CASE("buff rule BuffFloorTm AddFloorTime") { checkStatLatch(1230, "BuffFloorTm"); }
TEST_CASE("buff rule BuffModifyHPBasedByLostHp unsupported")
{
    checkUnsupported(2002, "BuffModifyHPBasedByLostHp");
    CHECK(lowerReport().find("15532401") != std::string::npos);
}

TEST_CASE("buff rule BuffHPMAX scales max and current hp")
{
    Harness h;
    const auto* def = findRule(2010);
    REQUIRE(def != nullptr);
    const auto* stat = firstKind(def->persistent, BuffOpKind::Stat);
    REQUIRE(stat != nullptr);
    CHECK(stat->stat == BuffStat::AddMaxHp);
    const Fixed p    = stat->value.a;
    auto& health     = h.world.ecs().get<HealthComponent>(h.self);
    const Fixed hp   = health.hp;
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AddMaxHp)) == hp * p);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp + hp * p);
    h.remove(def->id);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AddMaxHp)) == Fixed{});
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp == hp);
}

TEST_CASE("buff rule BuffHealingChange scales heal")
{
    Harness h;
    const auto* def = findRule(2020);
    REQUIRE(def != nullptr);
    const auto* stat = firstKind(def->persistent, BuffOpKind::Stat);
    REQUIRE(stat != nullptr);
    CHECK(stat->stat == BuffStat::HealingScale);
    auto& health   = h.world.ecs().get<HealthComponent>(h.self);
    health.hp    = Fixed::fromInt(5000);
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::HealingScale)) == stat->value.a);
    health::rules::applyDelta(h.world, h.self, Fixed::fromInt(100), HpCause::Heal, h.self);
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp ==
          Fixed::fromInt(5000) + Fixed::fromInt(100) * (Fixed::fromInt(1) + stat->value.a));
    h.remove(def->id);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::HealingScale)) == Fixed{});
}

TEST_CASE("buff rule BuffCPConsumeScale CpCostScale") { checkStatLatch(3030, "BuffCPConsumeScale"); }
TEST_CASE("buff rule BuffEPPlus SkillMod") { checkStatLatch(3040, "BuffEPPlus"); }

TEST_CASE("buff rule BuffTpAddNewBuff Apply on UseTp")
{
    Harness h;
    const auto* def = findRule(4001);
    REQUIRE(def != nullptr);
    const auto* apply = firstKind(def->onEnter, BuffOpKind::Apply);
    const auto* begin = firstKind(def->onBegin, BuffOpKind::Apply);
    const auto* op    = apply ? apply : begin;
    REQUIRE(op != nullptr);
    REQUIRE_FALSE(op->ids.empty());
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    bool child = false;
    for (const int32_t id : op->ids)
        if (findBuff(h.world, h.self, id) != nullptr)
            child = true;
    CHECK(child);
}

TEST_CASE("buff rule BuffTPRecoveryScaleBySpeed SpeedBonus") { checkStatLatch(4031, "BuffTPRecoveryScaleBySpeed"); }
TEST_CASE("buff rule BuffTPRecoveryScaleBySlotCD CoolingSlots live") { checkStatLatch(4032, "BuffTPRecoveryScaleBySlotCD"); }
TEST_CASE("buff rule BuffUpdateArtifactCD noop artifact") { checkNoop(5001, "BuffUpdateArtifactCD"); }

TEST_CASE("buff rule BuffHemophagia heals on Begin")
{
    Harness h;
    const auto* def = findRule(5020);
    REQUIRE(def != nullptr);
    const auto* res = firstKind(def->onBegin, BuffOpKind::Resource);
    REQUIRE(res != nullptr);
    CHECK(res->res == ResourceId::Hp);
    auto& health = h.world.ecs().get<HealthComponent>(h.self);
    health.hp  = Fixed::fromInt(4000);
    BuffContext ctx;
    ctx.damage   = Fixed::fromInt(200);
    ctx.critical = true;
    ctx.target   = h.other;
    activate(h, def->id, ctx);
    // 参照回血走表现血球到达；新 sim 在 Begin 直接 Resource，一次性行也可能已销毁实例。
    CHECK(h.world.ecs().get<HealthComponent>(h.self).hp > Fixed::fromInt(4000));
}

TEST_CASE("buff rule BuffModifyCDSkill SetRatio")
{
    Harness h;
    const auto* def = findRule(5030);
    REQUIRE(def != nullptr);
    seedBaseSlots(h, 1);
    const auto* cd    = firstKind(def->onEnter, BuffOpKind::Cooldown);
    const auto* begin = firstKind(def->onBegin, BuffOpKind::Cooldown);
    const auto* op    = cd ? cd : begin;
    REQUIRE(op != nullptr);
    CHECK(op->cdMode == CdMode::SetRatio);
    activate(h, def->id);
    h.setTickSeconds(1);
    h.tickBuff(1);
    const auto& skill = h.world.ecs().get<SkillBookComponent>(h.self).skills.front();
    CHECK(skill.cd != Fixed::fromInt(4000));
}

TEST_CASE("buff rule BuffConsumeCrystal noop crystal") { checkNoop(5040, "BuffConsumeCrystal"); }
TEST_CASE("buff rule BuffCritHPLoss Lost Hp") { checkStatLatch(5060, "BuffCritHPLoss"); }
TEST_CASE("buff rule BuffDeductEpReplaceHp unsupported")
{
    checkUnsupported(5080, "BuffDeductEpReplaceHp");
    CHECK(lowerReport().find("15220") != std::string::npos);
}
TEST_CASE("buff rule BuffDeductTpReplaceHp unsupported")
{
    checkUnsupported(5090, "BuffDeductTpReplaceHp");
    CHECK(lowerReport().find("battle_buff 5090") != std::string::npos);
}

TEST_CASE("buff rule BuffImmune blocks listed rules")
{
    Harness h;
    const auto* def = findRule(5100);
    REQUIRE(def != nullptr);
    const auto* immune = firstKind(def->persistent, BuffOpKind::Immune);
    REQUIRE(immune != nullptr);
    REQUIRE_FALSE(immune->ids.empty());
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    BuffDef victim                = makeBuff(nextBuffId());
    victim.admission.ruleId   = immune->ids.front();
    victim.persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t victimId = h.add(victim);
    h.admit(victimId);
    CHECK(findBuff(h.world, h.self, victimId) == nullptr);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Stealth));
    h.remove(def->id);
    h.admit(victimId);
    CHECK(findBuff(h.world, h.self, victimId) != nullptr);
}

TEST_CASE("buff rule BuffExtendDebuff raises maxTimes")
{
    Harness h;
    const auto* def = findRule(5120);
    REQUIRE(def != nullptr);
    const auto* ext   = firstKind(def->onEnter, BuffOpKind::Extend);
    const auto* begin = firstKind(def->onBegin, BuffOpKind::Extend);
    const auto* op    = ext ? ext : begin;
    REQUIRE(op != nullptr);
    BuffDef timed                   = makeBuff(nextBuffId());
    timed.admission.ruleId      = op->i0;
    timed.lifecycle.times       = 2;
    timed.lifecycle.intervalSec = Fixed::fromInt(10);
    const int32_t timedId           = h.add(timed);
    h.admit(timedId);
    REQUIRE(findBuff(h.world, h.self, timedId) != nullptr);
    CHECK(findBuff(h.world, h.self, timedId)->maxTimes == 2);
    activate(h, def->id);
    CHECK(findBuff(h.world, h.self, timedId)->maxTimes == 2 + op->i1);
}

TEST_CASE("buff rule BuffNotConsumeCrystal noop crystal") { checkNoop(5130, "BuffNotConsumeCrystal"); }

TEST_CASE("buff rule BuffWithTypeAddBuff Count then Apply")
{
    Harness h;
    const auto* def = findRule(7001);
    REQUIRE(def != nullptr);
    const auto* count = firstKind(def->onEnter, BuffOpKind::Count);
    const auto* apply = firstKind(def->onEnter, BuffOpKind::Apply);
    REQUIRE(count != nullptr);
    REQUIRE(apply != nullptr);
    REQUIRE_FALSE(apply->ids.empty());
    BuffDef mark = makeBuff(nextBuffId());
    if (count->countBy == CountBy::Type)
        mark.admission.buffType = count->i0;
    else if (count->countBy == CountBy::Rule)
        mark.admission.ruleId = count->i0;
    mark.admission.maxStacks = std::max(count->i1, 1);
    const int32_t markId         = h.add(mark);
    h.admit(markId);
    auto* inst = findBuff(h.world, h.self, markId);
    REQUIRE(inst != nullptr);
    inst->stacks = std::max(count->i1, 1);
    activate(h, def->id);
    CHECK(findBuff(h.world, h.self, apply->ids.front()) != nullptr);
}

TEST_CASE("buff rule BuffWithTypeAddBuff2 Count equal then Apply")
{
    Harness h;
    const auto* def = findRule(7003);
    REQUIRE(def != nullptr);
    const auto* count = firstKind(def->onEnter, BuffOpKind::Count);
    if (!count)
        count = firstKind(def->onBegin, BuffOpKind::Count);
    const auto* apply = firstKind(def->onEnter, BuffOpKind::Apply);
    if (!apply)
        apply = firstKind(def->onBegin, BuffOpKind::Apply);
    REQUIRE(count != nullptr);
    REQUIRE(apply != nullptr);
    REQUIRE_FALSE(apply->ids.empty());
    BuffDef mark = makeBuff(nextBuffId());
    if (count->countBy == CountBy::Type)
        mark.admission.buffType = count->i0;
    else if (count->countBy == CountBy::Rule)
        mark.admission.ruleId = count->i0;
    else if (count->countBy == CountBy::Id)
        mark.id = count->i0;
    mark.admission.maxStacks = std::max(count->i1, 1);
    const int32_t markId         = h.add(mark);
    h.admit(markId);
    auto* inst = findBuff(h.world, h.self, markId);
    REQUIRE(inst != nullptr);
    inst->stacks = count->i1;
    activate(h, def->id);
    CHECK(findBuff(h.world, h.self, apply->ids.front()) != nullptr);
}

TEST_CASE("buff rule BuffResetSkillCD resets base slots")
{
    Harness h;
    const auto* def = findRule(9000);
    REQUIRE(def != nullptr);
    seedBaseSlots(h, 3);
    CHECK(coolingCount(h.world, h.self) == 3);
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    CHECK(coolingCount(h.world, h.self) < 3);
}

TEST_CASE("buff rule BuffResetArtifactCD noop artifact") { checkNoop(9001, "BuffResetArtifactCD"); }

TEST_CASE("buff rule BuffModifyCDSkillNew PerSlot")
{
    Harness h;
    const auto* def = findRule(9002);
    REQUIRE(def != nullptr);
    seedBaseSlots(h, 2);
    const auto* cd    = firstKind(def->onEnter, BuffOpKind::Cooldown);
    const auto* begin = firstKind(def->onBegin, BuffOpKind::Cooldown);
    const auto* op    = cd ? cd : begin;
    REQUIRE(op != nullptr);
    CHECK(op->cdMode == CdMode::PerSlot);
    auto& book = h.world.ecs().get<SkillBookComponent>(h.self);
    for (const int32_t slot : op->ids)
    {
        bool have = false;
        for (const auto& skill : book.skills)
            if (skill.slot == slot)
                have = true;
        if (have)
            continue;
        SkillRuntime skill;
        skill.slot        = slot;
        skill.configId    = 920000;
        skill.cd          = Fixed::fromInt(4000);
        skill.coldMaxTime = Fixed::fromInt(8000);
        skill.timing      = true;
        book.skills.push_back(skill);
    }
    const int32_t targetSlot = op->ids.empty() ? 0 : op->ids.front();
    Fixed before{};
    for (const auto& skill : book.skills)
        if (skill.slot == targetSlot)
        {
            before = skill.cd;
            break;
        }
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    Fixed after = before;
    for (const auto& skill : h.world.ecs().get<SkillBookComponent>(h.self).skills)
        if (skill.slot == targetSlot)
        {
            after = skill.cd;
            break;
        }
    CHECK(after != before);
}

TEST_CASE("buff rule BuffCDArtifact noop artifact") { checkNoop(10003, "BuffCDArtifact"); }
TEST_CASE("buff rule BuffDisableArt noop artifact") { checkNoop(20002, "BuffDisableArt"); }
TEST_CASE("buff rule BuffDisableSlots SkillMod") { checkStatLatch(20003, "BuffDisableSlots"); }
TEST_CASE("buff rule BuffChangeRoleAcitonScaleTime ActionScale")
{
    Harness h;
    const auto* def = findRule(21001);
    REQUIRE(def != nullptr);
    const auto* stat = firstKind(def->persistent, BuffOpKind::Stat);
    REQUIRE(stat != nullptr);
    CHECK(stat->stat == BuffStat::ActionScale);
    seedBaseSlots(h, 0);
    h.world.ecs().get<SkillCastComponent>(h.self).skill = 0;
    const Predicate* action = nullptr;
    for (const auto& pred : def->activation.conditions)
        if (pred.kind == PredKind::Skill && pred.subKind == static_cast<int32_t>(SkillCheck::ActionId))
            action = &pred;
    if (action && !action->ids.empty())
        h.world.ecs().get<ActionTimelineComponent>(h.self).clipId = action->ids.front();
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::ActionScale)) != Fixed{});
}

TEST_CASE("buff rule BuffSealSkill Choice disable")
{
    Harness h;
    const auto* def = findRule(21002);
    REQUIRE(def != nullptr);
    seedBaseSlots(h, 0);
    const auto* choice = firstKind(def->persistent, BuffOpKind::Choice);
    REQUIRE(choice != nullptr);
    activate(h, def->id);
    auto* inst = findBuff(h.world, h.self, def->id);
    REQUIRE(inst != nullptr);
    CHECK(inst->choice >= 0);
    CHECK(inst->latched);
    h.remove(def->id);
    CHECK(findBuff(h.world, h.self, def->id) == nullptr);
    CHECK(h.world.ecs().get<SkillBookComponent>(h.self).mods.empty());
}

TEST_CASE("buff rule BuffSummonDummy AvoidHurt HpLock SummonLink")
{
    Harness h;
    const auto* def = findRule(21003);
    REQUIRE(def != nullptr);
    CHECK(hasActiveKind(*def, BuffOpKind::SummonLink));
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AvoidHurt)) != Fixed{});
    CHECK(statSum(h.world, h.self, kHpLockStat) != Fixed{});
    h.remove(def->id);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AvoidHurt)) == Fixed{});
    CHECK(statSum(h.world, h.self, kHpLockStat) == Fixed{});
}

TEST_CASE("buff rule BuffDummyDeathRattle OnSummonDeath Apply")
{
    Harness h;
    const auto* def = findRule(21004);
    REQUIRE(def != nullptr);
    const auto* pulse = firstKind(def->persistent, BuffOpKind::OnSummonDeath);
    REQUIRE(pulse != nullptr);
    activate(h, def->id);
    REQUIRE(findBuff(h.world, h.self, def->id) != nullptr);
    buff::rules::notifySummonDeath(h.world, h.self);
    if (!pulse->ids.empty())
        CHECK(findBuff(h.world, h.self, pulse->ids.front()) != nullptr);
}

TEST_CASE("buff 100830 BuffLoopCheck2 saturates innerCd without Fixed overflow")
{
    // 100830 的 innerCd=1e16 在定点里饱和到最大值；旧实现 tick 时做 innerElapsed += dt 触发
    // Fixed operator+ 溢出断言（Fixed.h:104）。新 sim 已在该加法前比较，饱和值不会再溢出。
    // 拆出 buff_spawn 后，此处单测覆盖其准入与"无子 Buff"的循环检查语义。
    Harness h;
    h.admit(100830);
    REQUIRE(findBuff(h.world, h.self, 100830) != nullptr);
    // BuffLoopCheck2 的 Watch 边沿模式在条件通过时移除子 Buff（而非追加），故无子 Buff 出现。
    CHECK(findBuff(h.world, h.self, 100831) == nullptr);
    CHECK(findBuff(h.world, h.self, 100832) == nullptr);
    h.tickWorld(3);
    REQUIRE(findBuff(h.world, h.self, 100830) != nullptr);
    CHECK(findBuff(h.world, h.self, 100831) == nullptr);
    CHECK(findBuff(h.world, h.self, 100832) == nullptr);
}
