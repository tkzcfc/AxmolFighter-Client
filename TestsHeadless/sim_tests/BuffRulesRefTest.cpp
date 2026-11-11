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
    int32_t m_id = 0;
    const char* m_name = nullptr;
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
    const auto& mask = row.m_admission.m_areaMask;
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
        if (row.m_admission.m_ruleId != ruleId)
            continue;
        int32_t score = 0;
        if (row.m_status != LowerStatus::Unsupported)
            score += 8;
        if (row.m_activation.m_conditions.empty())
            score += 16;
        if (row.m_lifecycle.m_executeType == 1)
            score += 4;
        if (row.m_lifecycle.m_executeType == 3 && row.m_activation.m_beginOn != TriggerPoint::None)
            score += 6;
        if (row.m_admission.m_probability >= Fixed::fromInt(100))
            score += 2;
        if (areaOkDefault(row))
            score += 12;
        if (!row.m_activation.m_oneShot)
            score += 2;
        else
            score -= 4;
        const int32_t enter = static_cast<int32_t>(BuffHook::Enter);
        const int32_t begin = static_cast<int32_t>(BuffHook::Begin);
        if (!row.m_onEnter.empty() || (row.m_activation.m_latchOn & enter) != 0)
            score += 4;
        if (row.m_activation.m_beginOn != TriggerPoint::None &&
            ((row.m_activation.m_latchOn & begin) != 0 || !row.m_onBegin.empty()))
            score += 6;
        if (row.m_activation.m_beginOn == TriggerPoint::None && row.m_onEnter.empty() &&
            (row.m_activation.m_latchOn & begin) != 0 && (row.m_activation.m_latchOn & enter) == 0)
            score -= 8;
        if (row.m_lifecycle.m_executeType == 2 && row.m_lifecycle.m_times == 1 &&
            row.m_lifecycle.m_intervalSec == Fixed{})
            score -= 4;
        for (const auto& op : row.m_persistent)
            if (op.m_effect == OpEffect::Active &&
                (op.m_kind == BuffOpKind::Stat || op.m_kind == BuffOpKind::HpLock) && op.m_value.m_a != Fixed{})
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
    for (const auto& op : def.m_persistent)
        fn(op);
    for (const auto& op : def.m_onEnter)
        fn(op);
    for (const auto& op : def.m_onTick)
        fn(op);
    for (const auto& op : def.m_onBegin)
        fn(op);
    for (const auto& op : def.m_onEnd)
        fn(op);
    for (const auto& op : def.m_onExit)
        fn(op);
    for (const auto& op : def.m_watch)
        fn(op);
    for (const auto& op : def.m_choicePool)
        fn(op);
}

bool hasActiveKind(const BuffDef& def, BuffOpKind kind)
{
    bool found = false;
    eachOp(def, [&](const BuffOp& op) {
        if (op.m_kind == kind && op.m_effect == OpEffect::Active)
            found = true;
    });
    return found;
}

const BuffOp* firstKind(const std::vector<BuffOp>& ops, BuffOpKind kind)
{
    for (const auto& op : ops)
        if (op.m_kind == kind)
            return &op;
    return nullptr;
}

void seedBaseSlots(Harness& h, int32_t cooling = 1)
{
    auto& book = h.world.ecs().get<SkillBook>(h.self);
    book.m_slots = {"A", "B", "C", "D", "E", "F", "Y"};
    book.m_skills.clear();
    for (int32_t i = 0; i < 7; ++i)
    {
        SkillRuntime skill;
        skill.m_slot        = i;
        skill.m_configId    = 920000;
        skill.m_cd          = Fixed::fromInt(4000);
        skill.m_coldMaxTime = Fixed::fromInt(8000);
        skill.m_timing      = i < cooling;
        book.m_skills.push_back(skill);
    }
}

void prepareValue(Harness& h, const ValueExpr& expr)
{
    if (expr.m_kind == ValueKind::Lost)
    {
        auto& health = h.world.ecs().get<Health>(h.self);
        if (expr.m_res == ResourceId::Hp)
            health.m_hp = health.m_maxHp / Fixed::fromInt(2);
        else if (expr.m_res == ResourceId::Mp)
            health.m_mp = health.m_maxMp / Fixed::fromInt(2);
        else if (expr.m_res == ResourceId::Ep)
            health.m_ep = health.m_maxEp / Fixed::fromInt(2);
    }
    else if (expr.m_kind == ValueKind::SpeedBonus)
    {
        h.world.ecs().get<Stats>(h.self).m_mods.push_back(
            {static_cast<int32_t>(BuffStat::MoveScaleX), "0.5"_fx, 0, false});
    }
    else if (expr.m_kind == ValueKind::ComboTable)
    {
        auto& combo = h.world.ecs().get<ComboMeter>(h.self);
        combo.m_grade = 0;
        for (int32_t i = static_cast<int32_t>(expr.m_table.size()) - 1; i >= 0; --i)
            if (expr.m_table[static_cast<size_t>(i)] != Fixed{})
            {
                combo.m_grade = i;
                break;
            }
    }
    else if (expr.m_kind == ValueKind::CoolingSlots || expr.m_kind == ValueKind::CoolingSlotTable)
        seedBaseSlots(h, 1);
    else if (expr.m_kind == ValueKind::TypeCount || expr.m_kind == ValueKind::MaxTimes)
    {
        for (const int32_t type : expr.m_types)
        {
            BuffDef mark                = makeBuff(nextBuffId());
            mark.m_admission.m_buffType = type;
            h.admit(h.add(mark));
        }
    }
}

void prepareDef(Harness& h, const BuffDef& def)
{
    eachOp(def, [&](const BuffOp& op) { prepareValue(h, op.m_value); });
}

BuffDef* mutableRule(int32_t id)
{
    for (auto& row : buffTable())
        if (row.m_id == id)
            return &row;
    return nullptr;
}

void armBegin(BuffDef& def)
{
    if (def.m_activation.m_beginOn != TriggerPoint::None)
        return;
    const int32_t enter = static_cast<int32_t>(BuffHook::Enter);
    if (def.m_lifecycle.m_executeType == 1 &&
        (!def.m_onEnter.empty() || (def.m_activation.m_latchOn & enter) != 0))
        return;
    const bool beginLatch = (def.m_activation.m_latchOn & static_cast<int32_t>(BuffHook::Begin)) != 0;
    if (beginLatch || !def.m_onBegin.empty())
        def.m_activation.m_beginOn = TriggerPoint::AfterHit;
}

void satisfy(Harness& h, const BuffDef& def, BuffContext& ctx)
{
    bool needSkill = false;
    auto applyPred = [&](const Predicate& pred) {
        if (pred.m_kind == PredKind::Skill)
        {
            needSkill = true;
            if (pred.m_subKind == static_cast<int32_t>(SkillCheck::ActionId))
            {
                auto& timeline = h.world.ecs().get<ActionTimeline>(h.self);
                timeline.m_clipId = pred.m_id != 0 ? pred.m_id : (pred.m_ids.empty() ? 0 : pred.m_ids.front());
            }
        }
        if (pred.m_kind == PredKind::Event)
        {
            if (pred.m_subKind == static_cast<int32_t>(EventCheck::Crit))
                ctx.m_critical = true;
            if (pred.m_subKind == static_cast<int32_t>(EventCheck::UseTp))
                ctx.m_tpSpent = pred.m_min > Fixed{} ? pred.m_min : Fixed::fromInt(20);
        }
    };
    for (const auto& pred : def.m_activation.m_conditions)
        applyPred(pred);
    if (needSkill)
    {
        if (h.world.ecs().get<SkillBook>(h.self).m_skills.empty())
            seedBaseSlots(h, 1);
        h.world.ecs().get<SkillCast>(h.self).m_skill = 0;
    }
    if (def.m_activation.m_eventFilter == 2)
        ctx.m_target = h.other;
}

void activate(Harness& h, int32_t id, const BuffContext& ctx = {})
{
    auto* live            = mutableRule(id);
    TriggerPoint saved    = TriggerPoint::None;
    if (live)
    {
        saved = live->m_activation.m_beginOn;
        armBegin(*live);
    }
    const auto* def = conf::Config::instance().find<BuffDef>(id);
    BuffContext use = ctx;
    if (def)
        satisfy(h, *def, use);
    h.admit(id, use);
    def = conf::Config::instance().find<BuffDef>(id);
    if (def && def->m_activation.m_beginOn != TriggerPoint::None)
        h.fire(def->m_activation.m_beginOn, use);
    if (live)
        live->m_activation.m_beginOn = saved;
}

bool anyFlag(World& world, Entity entity)
{
    const auto& flags = world.ecs().get<StatusFlags>(entity);
    for (int32_t i = 1; i <= 8; ++i)
        if (flags.active(static_cast<ExtraState>(i)))
            return true;
    return flags.m_noHeal > 0 || flags.m_noCrystal > 0;
}

int32_t coolingCount(World& world, Entity entity)
{
    int32_t n        = 0;
    const auto& book = world.ecs().get<SkillBook>(entity);
    for (const auto& skill : book.m_skills)
        if (skill.m_timing && skill.m_cd > Fixed{})
            ++n;
    return n;
}

void checkMarker(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    CHECK(def->m_status == LowerStatus::Lowered);
    CHECK(def->m_persistent.empty());
    CHECK_FALSE(hasActiveKind(*def, BuffOpKind::Dot));
    CHECK_FALSE(hasActiveKind(*def, BuffOpKind::Stat));
    const Fixed hp    = h.world.ecs().get<Health>(h.self).m_hp;
    const auto mods   = h.world.ecs().get<Stats>(h.self).m_mods.size();
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    CHECK(h.world.ecs().get<Health>(h.self).m_hp == hp);
    CHECK(h.world.ecs().get<Stats>(h.self).m_mods.size() == mods);
    CHECK_FALSE(anyFlag(h.world, h.self));
    CHECK(reportLists(name));
}

void checkDot(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    const auto* dot = firstKind(def->m_onEnter, BuffOpKind::Dot);
    REQUIRE(dot != nullptr);
    CHECK(dot->m_effect == OpEffect::Active);
    const Fixed hp = h.world.ecs().get<Health>(h.self).m_hp;
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    if (def->m_lifecycle.m_executeType == 2)
    {
        h.setTickSeconds(1);
        h.tickBuff(3);
    }
    const Fixed after = h.world.ecs().get<Health>(h.self).m_hp;
    if (dot->m_value.m_a < Fixed{})
        CHECK(after < hp);
    else if (dot->m_value.m_a > Fixed{})
        CHECK(after > hp);
    CHECK(reportLists(name));
}

void checkStatLatch(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    REQUIRE_FALSE(def->m_persistent.empty());
    prepareDef(h, *def);
    seedBaseSlots(h, 1);
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    int32_t checked = 0;
    for (const auto& op : def->m_persistent)
    {
        if (op.m_effect != OpEffect::Active)
            continue;
        if (op.m_kind == BuffOpKind::Stat)
        {
            CHECK(statSum(h.world, h.self, static_cast<int32_t>(op.m_stat)) != Fixed{});
            ++checked;
        }
        else if (op.m_kind == BuffOpKind::SkillMod)
        {
            CHECK_FALSE(h.world.ecs().get<SkillBook>(h.self).m_mods.empty());
            ++checked;
        }
        else if (op.m_kind == BuffOpKind::Flag)
        {
            if (op.m_flag == BuffFlag::NoHeal)
                CHECK(h.world.ecs().get<StatusFlags>(h.self).m_noHeal > 0);
            else if (op.m_flag == BuffFlag::NoCrystalCost)
                CHECK(h.world.ecs().get<StatusFlags>(h.self).m_noCrystal > 0);
            else
                CHECK(hasFlag(h.world, h.self, static_cast<ExtraState>(op.m_flag)));
            ++checked;
        }
        else if (op.m_kind == BuffOpKind::Immune)
        {
            CHECK_FALSE(h.world.ecs().get<BuffList>(h.self).m_immunity.empty());
            ++checked;
        }
        else if (op.m_kind == BuffOpKind::HpLock)
        {
            CHECK(statSum(h.world, h.self, kHpLockStat) != Fixed{});
            ++checked;
        }
    }
    CHECK(checked > 0);
    h.remove(def->m_id);
    CHECK(findBuff(h.world, h.self, def->m_id) == nullptr);
    for (const auto& op : def->m_persistent)
    {
        if (op.m_effect != OpEffect::Active || op.m_kind != BuffOpKind::Stat)
            continue;
        CHECK(statSum(h.world, h.self, static_cast<int32_t>(op.m_stat)) == Fixed{});
    }
    CHECK(reportLists(name));
}

void checkNoop(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    CHECK(def->m_status == LowerStatus::Noop);
    CHECK(reportLists(name));
    const Fixed hp    = h.world.ecs().get<Health>(h.self).m_hp;
    const auto mods   = h.world.ecs().get<Stats>(h.self).m_mods.size();
    const auto skills = h.world.ecs().get<SkillBook>(h.self).m_mods.size();
    h.admit(def->m_id);
    CHECK(h.world.ecs().get<Health>(h.self).m_hp == hp);
    CHECK(h.world.ecs().get<Stats>(h.self).m_mods.size() == mods);
    CHECK(h.world.ecs().get<SkillBook>(h.self).m_mods.size() == skills);
    CHECK_FALSE(anyFlag(h.world, h.self));
}

void checkUnsupported(int32_t ruleId, const char* name)
{
    Harness h;
    const auto* def = findRule(ruleId);
    REQUIRE(def != nullptr);
    CHECK(def->m_status == LowerStatus::Unsupported);
    CHECK(reportLists(name));
    CHECK(lowerReport().find("unsupported") != std::string::npos);
    const Fixed hp = h.world.ecs().get<Health>(h.self).m_hp;
    h.admit(def->m_id);
    CHECK(findBuff(h.world, h.self, def->m_id) == nullptr);
    CHECK(h.world.ecs().get<Health>(h.self).m_hp == hp);
    CHECK_FALSE(anyFlag(h.world, h.self));
}
}  // namespace

TEST_CASE("unported rule catalog is 64 and present in config.bin")
{
    buff_test::loadAssets();
    CHECK(sizeof(kUnported) / sizeof(kUnported[0]) == 64);
    for (const auto& rule : kUnported)
    {
        CAPTURE(rule.m_id);
        CAPTURE(rule.m_name);
        const auto* def = findRule(rule.m_id);
        REQUIRE(def != nullptr);
        CHECK(def->m_admission.m_ruleId == rule.m_id);
        CHECK(reportLists(rule.m_name));
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
    const auto* apply = firstKind(def->m_onEnter, BuffOpKind::Apply);
    REQUIRE(apply != nullptr);
    CHECK(apply->m_pick == PickMode::Weighted);
    REQUIRE_FALSE(apply->m_ids.empty());
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    bool child = false;
    for (const int32_t id : apply->m_ids)
        if (findBuff(h.world, h.self, id) != nullptr)
            child = true;
    CHECK(child);
}

TEST_CASE("buff rule BuffRemoveByTypeAddExtra counts then mods")
{
    Harness h;
    const auto* def = findRule(401);
    REQUIRE(def != nullptr);
    const auto* remove = firstKind(def->m_onEnter, BuffOpKind::Remove);
    REQUIRE(remove != nullptr);
    REQUIRE_FALSE(remove->m_ids.empty());
    BuffDef mark                = makeBuff(nextBuffId());
    mark.m_admission.m_buffType = remove->m_ids.front();
    const int32_t markId        = h.add(mark);
    h.admit(markId);
    REQUIRE(findBuff(h.world, h.self, markId) != nullptr);
    auto& health = h.world.ecs().get<Health>(h.self);
    health.m_mp  = Fixed::fromInt(10);
    activate(h, def->m_id);
    CHECK(findBuff(h.world, h.self, markId) == nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AddHurt)) != Fixed{});
}

TEST_CASE("buff rule BuffAddByComboEvaluate watches kids")
{
    Harness h;
    const auto* def = findRule(600);
    REQUIRE(def != nullptr);
    REQUIRE_FALSE(def->m_watch.empty());
    const auto& watch = def->m_watch.front();
    CHECK(watch.m_kind == BuffOpKind::Watch);
    CHECK(watch.m_watch == WatchMode::Latched);
    REQUIRE_FALSE(watch.m_ids.empty());
    const int32_t kid = watch.m_ids.front();
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    // 先满足条件合上门闩（bits=1、不挂子），再落到未满足以挂子。
    h.world.ecs().get<ComboMeter>(h.self).m_grade = 99;
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, kid) == nullptr);
    h.world.ecs().get<ComboMeter>(h.self).m_grade = 0;
    h.tickBuff();
    CHECK(findBuff(h.world, h.self, kid) != nullptr);
    h.world.ecs().get<ComboMeter>(h.self).m_grade = 99;
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
    auto& health = h.world.ecs().get<Health>(h.self);
    health.m_hp  = health.m_maxHp / Fixed::fromInt(2);
    const Fixed hp = health.m_hp;
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    CHECK(h.world.ecs().get<Health>(h.self).m_hp != hp);
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
    const auto* stat = firstKind(def->m_persistent, BuffOpKind::Stat);
    REQUIRE(stat != nullptr);
    CHECK(stat->m_stat == BuffStat::AddMaxHp);
    const Fixed p    = stat->m_value.m_a;
    auto& health     = h.world.ecs().get<Health>(h.self);
    const Fixed hp   = health.m_hp;
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AddMaxHp)) == hp * p);
    CHECK(h.world.ecs().get<Health>(h.self).m_hp == hp + hp * p);
    h.remove(def->m_id);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AddMaxHp)) == Fixed{});
    CHECK(h.world.ecs().get<Health>(h.self).m_hp == hp);
}

TEST_CASE("buff rule BuffHealingChange scales heal")
{
    Harness h;
    const auto* def = findRule(2020);
    REQUIRE(def != nullptr);
    const auto* stat = firstKind(def->m_persistent, BuffOpKind::Stat);
    REQUIRE(stat != nullptr);
    CHECK(stat->m_stat == BuffStat::HealingScale);
    auto& health   = h.world.ecs().get<Health>(h.self);
    health.m_hp    = Fixed::fromInt(5000);
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::HealingScale)) == stat->m_value.m_a);
    health::rules::applyDelta(h.world, h.self, Fixed::fromInt(100), HpCause::Heal, h.self);
    CHECK(h.world.ecs().get<Health>(h.self).m_hp ==
          Fixed::fromInt(5000) + Fixed::fromInt(100) * (Fixed::fromInt(1) + stat->m_value.m_a));
    h.remove(def->m_id);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::HealingScale)) == Fixed{});
}

TEST_CASE("buff rule BuffCPConsumeScale CpCostScale") { checkStatLatch(3030, "BuffCPConsumeScale"); }
TEST_CASE("buff rule BuffEPPlus SkillMod") { checkStatLatch(3040, "BuffEPPlus"); }

TEST_CASE("buff rule BuffTpAddNewBuff Apply on UseTp")
{
    Harness h;
    const auto* def = findRule(4001);
    REQUIRE(def != nullptr);
    const auto* apply = firstKind(def->m_onEnter, BuffOpKind::Apply);
    const auto* begin = firstKind(def->m_onBegin, BuffOpKind::Apply);
    const auto* op    = apply ? apply : begin;
    REQUIRE(op != nullptr);
    REQUIRE_FALSE(op->m_ids.empty());
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    bool child = false;
    for (const int32_t id : op->m_ids)
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
    const auto* res = firstKind(def->m_onBegin, BuffOpKind::Resource);
    REQUIRE(res != nullptr);
    CHECK(res->m_res == ResourceId::Hp);
    auto& health = h.world.ecs().get<Health>(h.self);
    health.m_hp  = Fixed::fromInt(4000);
    BuffContext ctx;
    ctx.m_damage   = Fixed::fromInt(200);
    ctx.m_critical = true;
    ctx.m_target   = h.other;
    activate(h, def->m_id, ctx);
    // 参照回血走表现血球到达；新 sim 在 Begin 直接 Resource，一次性行也可能已销毁实例。
    CHECK(h.world.ecs().get<Health>(h.self).m_hp > Fixed::fromInt(4000));
}

TEST_CASE("buff rule BuffModifyCDSkill SetRatio")
{
    Harness h;
    const auto* def = findRule(5030);
    REQUIRE(def != nullptr);
    seedBaseSlots(h, 1);
    const auto* cd    = firstKind(def->m_onEnter, BuffOpKind::Cooldown);
    const auto* begin = firstKind(def->m_onBegin, BuffOpKind::Cooldown);
    const auto* op    = cd ? cd : begin;
    REQUIRE(op != nullptr);
    CHECK(op->m_cdMode == CdMode::SetRatio);
    activate(h, def->m_id);
    h.setTickSeconds(1);
    h.tickBuff(1);
    const auto& skill = h.world.ecs().get<SkillBook>(h.self).m_skills.front();
    CHECK(skill.m_cd != Fixed::fromInt(4000));
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
    const auto* immune = firstKind(def->m_persistent, BuffOpKind::Immune);
    REQUIRE(immune != nullptr);
    REQUIRE_FALSE(immune->m_ids.empty());
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    BuffDef victim                = makeBuff(nextBuffId());
    victim.m_admission.m_ruleId   = immune->m_ids.front();
    victim.m_persistent.push_back(flagOp(BuffFlag::Stealth));
    const int32_t victimId = h.add(victim);
    h.admit(victimId);
    CHECK(findBuff(h.world, h.self, victimId) == nullptr);
    CHECK_FALSE(hasFlag(h.world, h.self, ExtraState::Stealth));
    h.remove(def->m_id);
    h.admit(victimId);
    CHECK(findBuff(h.world, h.self, victimId) != nullptr);
}

TEST_CASE("buff rule BuffExtendDebuff raises maxTimes")
{
    Harness h;
    const auto* def = findRule(5120);
    REQUIRE(def != nullptr);
    const auto* ext   = firstKind(def->m_onEnter, BuffOpKind::Extend);
    const auto* begin = firstKind(def->m_onBegin, BuffOpKind::Extend);
    const auto* op    = ext ? ext : begin;
    REQUIRE(op != nullptr);
    BuffDef timed                   = makeBuff(nextBuffId());
    timed.m_admission.m_ruleId      = op->m_i0;
    timed.m_lifecycle.m_times       = 2;
    timed.m_lifecycle.m_intervalSec = Fixed::fromInt(10);
    const int32_t timedId           = h.add(timed);
    h.admit(timedId);
    REQUIRE(findBuff(h.world, h.self, timedId) != nullptr);
    CHECK(findBuff(h.world, h.self, timedId)->m_maxTimes == 2);
    activate(h, def->m_id);
    CHECK(findBuff(h.world, h.self, timedId)->m_maxTimes == 2 + op->m_i1);
}

TEST_CASE("buff rule BuffNotConsumeCrystal noop crystal") { checkNoop(5130, "BuffNotConsumeCrystal"); }

TEST_CASE("buff rule BuffWithTypeAddBuff Count then Apply")
{
    Harness h;
    const auto* def = findRule(7001);
    REQUIRE(def != nullptr);
    const auto* count = firstKind(def->m_onEnter, BuffOpKind::Count);
    const auto* apply = firstKind(def->m_onEnter, BuffOpKind::Apply);
    REQUIRE(count != nullptr);
    REQUIRE(apply != nullptr);
    REQUIRE_FALSE(apply->m_ids.empty());
    BuffDef mark = makeBuff(nextBuffId());
    if (count->m_countBy == CountBy::Type)
        mark.m_admission.m_buffType = count->m_i0;
    else if (count->m_countBy == CountBy::Rule)
        mark.m_admission.m_ruleId = count->m_i0;
    mark.m_admission.m_maxStacks = std::max(count->m_i1, 1);
    const int32_t markId         = h.add(mark);
    h.admit(markId);
    auto* inst = findBuff(h.world, h.self, markId);
    REQUIRE(inst != nullptr);
    inst->m_stacks = std::max(count->m_i1, 1);
    activate(h, def->m_id);
    CHECK(findBuff(h.world, h.self, apply->m_ids.front()) != nullptr);
}

TEST_CASE("buff rule BuffWithTypeAddBuff2 Count equal then Apply")
{
    Harness h;
    const auto* def = findRule(7003);
    REQUIRE(def != nullptr);
    const auto* count = firstKind(def->m_onEnter, BuffOpKind::Count);
    if (!count)
        count = firstKind(def->m_onBegin, BuffOpKind::Count);
    const auto* apply = firstKind(def->m_onEnter, BuffOpKind::Apply);
    if (!apply)
        apply = firstKind(def->m_onBegin, BuffOpKind::Apply);
    REQUIRE(count != nullptr);
    REQUIRE(apply != nullptr);
    REQUIRE_FALSE(apply->m_ids.empty());
    BuffDef mark = makeBuff(nextBuffId());
    if (count->m_countBy == CountBy::Type)
        mark.m_admission.m_buffType = count->m_i0;
    else if (count->m_countBy == CountBy::Rule)
        mark.m_admission.m_ruleId = count->m_i0;
    else if (count->m_countBy == CountBy::Id)
        mark.m_id = count->m_i0;
    mark.m_admission.m_maxStacks = std::max(count->m_i1, 1);
    const int32_t markId         = h.add(mark);
    h.admit(markId);
    auto* inst = findBuff(h.world, h.self, markId);
    REQUIRE(inst != nullptr);
    inst->m_stacks = count->m_i1;
    activate(h, def->m_id);
    CHECK(findBuff(h.world, h.self, apply->m_ids.front()) != nullptr);
}

TEST_CASE("buff rule BuffResetSkillCD resets base slots")
{
    Harness h;
    const auto* def = findRule(9000);
    REQUIRE(def != nullptr);
    seedBaseSlots(h, 3);
    CHECK(coolingCount(h.world, h.self) == 3);
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    CHECK(coolingCount(h.world, h.self) < 3);
}

TEST_CASE("buff rule BuffResetArtifactCD noop artifact") { checkNoop(9001, "BuffResetArtifactCD"); }

TEST_CASE("buff rule BuffModifyCDSkillNew PerSlot")
{
    Harness h;
    const auto* def = findRule(9002);
    REQUIRE(def != nullptr);
    seedBaseSlots(h, 2);
    const auto* cd    = firstKind(def->m_onEnter, BuffOpKind::Cooldown);
    const auto* begin = firstKind(def->m_onBegin, BuffOpKind::Cooldown);
    const auto* op    = cd ? cd : begin;
    REQUIRE(op != nullptr);
    CHECK(op->m_cdMode == CdMode::PerSlot);
    auto& book = h.world.ecs().get<SkillBook>(h.self);
    for (const int32_t slot : op->m_ids)
    {
        bool have = false;
        for (const auto& skill : book.m_skills)
            if (skill.m_slot == slot)
                have = true;
        if (have)
            continue;
        SkillRuntime skill;
        skill.m_slot        = slot;
        skill.m_configId    = 920000;
        skill.m_cd          = Fixed::fromInt(4000);
        skill.m_coldMaxTime = Fixed::fromInt(8000);
        skill.m_timing      = true;
        book.m_skills.push_back(skill);
    }
    const int32_t targetSlot = op->m_ids.empty() ? 0 : op->m_ids.front();
    Fixed before{};
    for (const auto& skill : book.m_skills)
        if (skill.m_slot == targetSlot)
        {
            before = skill.m_cd;
            break;
        }
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    Fixed after = before;
    for (const auto& skill : h.world.ecs().get<SkillBook>(h.self).m_skills)
        if (skill.m_slot == targetSlot)
        {
            after = skill.m_cd;
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
    const auto* stat = firstKind(def->m_persistent, BuffOpKind::Stat);
    REQUIRE(stat != nullptr);
    CHECK(stat->m_stat == BuffStat::ActionScale);
    seedBaseSlots(h, 0);
    h.world.ecs().get<SkillCast>(h.self).m_skill = 0;
    const Predicate* action = nullptr;
    for (const auto& pred : def->m_activation.m_conditions)
        if (pred.m_kind == PredKind::Skill && pred.m_subKind == static_cast<int32_t>(SkillCheck::ActionId))
            action = &pred;
    if (action && !action->m_ids.empty())
        h.world.ecs().get<ActionTimeline>(h.self).m_clipId = action->m_ids.front();
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::ActionScale)) != Fixed{});
}

TEST_CASE("buff rule BuffSealSkill Choice disable")
{
    Harness h;
    const auto* def = findRule(21002);
    REQUIRE(def != nullptr);
    seedBaseSlots(h, 0);
    const auto* choice = firstKind(def->m_persistent, BuffOpKind::Choice);
    REQUIRE(choice != nullptr);
    activate(h, def->m_id);
    auto* inst = findBuff(h.world, h.self, def->m_id);
    REQUIRE(inst != nullptr);
    CHECK(inst->m_choice >= 0);
    CHECK(inst->m_latched);
    h.remove(def->m_id);
    CHECK(findBuff(h.world, h.self, def->m_id) == nullptr);
    CHECK(h.world.ecs().get<SkillBook>(h.self).m_mods.empty());
}

TEST_CASE("buff rule BuffSummonDummy AvoidHurt HpLock SummonLink")
{
    Harness h;
    const auto* def = findRule(21003);
    REQUIRE(def != nullptr);
    CHECK(hasActiveKind(*def, BuffOpKind::SummonLink));
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AvoidHurt)) != Fixed{});
    CHECK(statSum(h.world, h.self, kHpLockStat) != Fixed{});
    h.remove(def->m_id);
    CHECK(statSum(h.world, h.self, static_cast<int32_t>(BuffStat::AvoidHurt)) == Fixed{});
    CHECK(statSum(h.world, h.self, kHpLockStat) == Fixed{});
}

TEST_CASE("buff rule BuffDummyDeathRattle OnSummonDeath Apply")
{
    Harness h;
    const auto* def = findRule(21004);
    REQUIRE(def != nullptr);
    const auto* pulse = firstKind(def->m_persistent, BuffOpKind::OnSummonDeath);
    REQUIRE(pulse != nullptr);
    activate(h, def->m_id);
    REQUIRE(findBuff(h.world, h.self, def->m_id) != nullptr);
    buff::rules::notifySummonDeath(h.world, h.self);
    if (!pulse->m_ids.empty())
        CHECK(findBuff(h.world, h.self, pulse->m_ids.front()) != nullptr);
}
