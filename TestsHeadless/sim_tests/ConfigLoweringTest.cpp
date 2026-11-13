#include "doctest.h"
#include "mugen/conf/ReflectedConfig.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace mugen;
using namespace mugen::conf;

namespace
{
// 读取 Content/mugen/config/config.bin 的原始字节。
std::vector<uint8_t> readConfigBytes()
{
    const std::string path = std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// 加载一次配置，供全部测试用例共享。
const Config& loadedConfig()
{
    static const bool ok = Config::instance().loadBytes(readConfigBytes());
    REQUIRE(ok);
    return Config::instance();
}

// 按 id 便捷查找。
template <class T>
const T* find(int32_t id)
{
    return loadedConfig().find<T>(id);
}

// 在 cue 列表里查找符合谓词的 cue。
template <class Pred>
bool hasCue(const std::vector<battle::ClipCue>& cues, Pred pred)
{
    for (const auto& cue : cues)
        if (pred(cue))
            return true;
    return false;
}
}  // namespace

TEST_CASE("battle lowering: derived tables are loaded")
{
    CHECK_FALSE(loadedConfig().table<battle::SkillDef>().empty());
    CHECK_FALSE(loadedConfig().table<battle::ClipDef>().empty());
    CHECK_FALSE(loadedConfig().table<battle::EffectClipDef>().empty());
    CHECK_FALSE(loadedConfig().table<battle::EffectDef>().empty());
    CHECK_FALSE(loadedConfig().table<battle::HitDef>().empty());
    CHECK_FALSE(loadedConfig().table<battle::RoleDef>().empty());
    CHECK_FALSE(loadedConfig().table<battle::BuffDef>().empty());
    CHECK_FALSE(loadedConfig().table<battle::AbilityDef>().empty());
}

TEST_CASE("battle lowering: A-chain skill nextSkill chain and sequences")
{
    const auto* s0 = find<battle::SkillDef>(920000);
    REQUIRE(s0 != nullptr);
    CHECK(s0->next == 920010);
    REQUIRE(s0->sequences.size() == 1);
    REQUIRE(s0->sequences[0].size() == 3);
    CHECK(s0->sequences[0][0] == 920000);
    CHECK(s0->sequences[0][1] == 920010);
    CHECK(s0->sequences[0][2] == 920020);

    const auto* s1 = find<battle::SkillDef>(920010);
    REQUIRE(s1 != nullptr);
    CHECK(s1->next == 920020);
    REQUIRE(s1->sequences[0].size() == 3);
    CHECK(s1->sequences[0][0] == 920030);

    const auto* s2 = find<battle::SkillDef>(920020);
    REQUIRE(s2 != nullptr);
    CHECK(s2->next == 920030);
    REQUIRE(s2->sequences[0].size() == 4);

    const auto* s3 = find<battle::SkillDef>(920030);
    REQUIRE(s3 != nullptr);
    CHECK(s3->next == 920040);
    REQUIRE(s3->sequences[0].size() == 3);
    CHECK(s3->sequences[0][0] == 920100);

    const auto* s4 = find<battle::SkillDef>(920040);
    REQUIRE(s4 != nullptr);
    CHECK(s4->next == -1);
    REQUIRE(s4->sequences[0].size() == 3);
    CHECK(s4->sequences[0][0] == 920130);
    CHECK(s4->sequences[0][2] == 920150);
}

TEST_CASE("battle lowering: hold/release (charge) skill fields are wired")
{
    // 当前源表 skillType 全为 0（无抬起释放技能），字段仍按旧实现语义降级：
    // holdRelease = skillType==1、pressTimeMs = pressTime（毫秒）、releaseSequences = upActionIds。
    const auto* s0 = find<battle::SkillDef>(920000);
    REQUIRE(s0 != nullptr);
    CHECK(s0->holdRelease == false);     // skillType == 0
    CHECK(s0->pressTimeMs == 0);         // pressTime == 0
    REQUIRE(s0->releaseSequences.size() == 1); // upActionIds 一个方向列
    REQUIRE(s0->releaseSequences[0].size() == 1);
    CHECK(s0->releaseSequences[0][0] == 0);    // upActionIds = {{0}}
}

TEST_CASE("battle lowering: A-chain clips motion and cancel windows")
{
    // 片段 920000..920150 的动作索引依次为 10118..10148（步长 2）。
    for (int i = 0; i < 16; ++i)
    {
        const int32_t clipId = 920000 + i * 10;
        const auto* clip = find<battle::ClipDef>(clipId);
        REQUIRE(clip != nullptr);
        const auto* motion = std::get_if<int32_t>(&clip->motion);
        REQUIRE(motion != nullptr);
        CHECK(*motion == 10118 + 2 * i);
    }

    // 920010：普通取消窗口 = interruptFrame(2)，尾延时 = actionDelayTime(0)，
    // 第 1 帧生成特效 920000。
    const auto* c10 = find<battle::ClipDef>(920010);
    REQUIRE(c10 != nullptr);
    CHECK(c10->cancel.normalFrame == 2);
    CHECK(c10->tailDelayMs == 0);
    CHECK(hasCue(c10->cues, [](const battle::ClipCue& cue) {
        return cue.kind == battle::ClipCueKind::SpawnEffect && cue.frame == 1 && cue.effectId == 920000;
    }));

    // 920110：普通取消窗口 = interruptFrame(10)，尾延时 = 80，
    // 第 5 帧镜头 1、第 0 帧特效 920040。
    const auto* c110 = find<battle::ClipDef>(920110);
    REQUIRE(c110 != nullptr);
    CHECK(c110->cancel.normalFrame == 10);
    CHECK(c110->tailDelayMs == 80);
    CHECK(hasCue(c110->cues, [](const battle::ClipCue& cue) {
        return cue.kind == battle::ClipCueKind::CameraShake && cue.frame == 5 && cue.cameraId == 1;
    }));
    CHECK(hasCue(c110->cues, [](const battle::ClipCue& cue) {
        return cue.kind == battle::ClipCueKind::SpawnEffect && cue.frame == 0 && cue.effectId == 920040;
    }));
}

TEST_CASE("battle lowering: A-chain effects hitId / relativePosition / autoRelease")
{
    const auto* e0 = find<battle::EffectDef>(920000);
    REQUIRE(e0 != nullptr);
    CHECK(e0->hitId == 920000);
    CHECK(e0->autoRelease == 1);
    REQUIRE(e0->relativePosition.size() == 3);
    CHECK(e0->relativePosition[0] == Fixed::fromInt(0));

    const auto* e10 = find<battle::EffectDef>(920010);
    REQUIRE(e10 != nullptr);
    CHECK(e10->hitId == 920010);
    REQUIRE(e10->relativePosition.size() == 3);
    CHECK(e10->relativePosition[0] == Fixed::fromInt(-5));

    // 920050 没有命中配置（hitId = -1）；920060 复用 920050。
    CHECK(find<battle::EffectDef>(920050)->hitId == -1);
    CHECK(find<battle::EffectDef>(920060)->hitId == 920050);
}

TEST_CASE("battle lowering: A-chain hits freeze / hitType / stiffTime / displacement")
{
    const int32_t hitTypes[6] = {0, 0, 0, 2, 2, 2};
    const int32_t stiffTimes[6] = {100, 100, 250, 800, 800, 800};
    for (int i = 0; i < 6; ++i)
    {
        const int32_t hitId = 920000 + i * 10;
        const auto* hit = find<battle::HitDef>(hitId);
        REQUIRE(hit != nullptr);
        CHECK(hit->freezeTime == 20);
        CHECK(hit->hitType == hitTypes[i]);
        CHECK(hit->stiffTime == stiffTimes[i]);
    }

    const auto* h0 = find<battle::HitDef>(920000);
    CHECK(h0->displacementId == 920000);
    CHECK(h0->airDisplacementId == 920010);
    CHECK(h0->floorDisplacementId == 920020);
}

TEST_CASE("battle lowering: roles 101 and 61320")
{
    const auto* r101 = find<battle::RoleDef>(101);
    REQUIRE(r101 != nullptr);
    CHECK(r101->roleType == 1);
    CHECK(r101->radius == 20);
    CHECK(r101->rigidity == 24);
    CHECK(r101->fatigue == 200);
    CHECK(r101->hitCount == -1);
    CHECK(r101->hitStiffTime == -1);
    CHECK(r101->hitDisplacementId == -1);
    CHECK(r101->deathDisplacementId == 133);
    CHECK(r101->deathEffectId == 8);
    CHECK(r101->resSpineId == 88101);
    CHECK(r101->monsterCamps == 4);
    CHECK(r101->velocity == "0.25"_fx);
    CHECK(r101->weight == "0.05"_fx);
    REQUIRE(r101->aiId.size() == 1);
    CHECK(r101->aiId[0] == 11);
    REQUIRE(r101->attributeRate.size() == 16);
    REQUIRE(r101->hurtNumPos.size() == 2);
    CHECK(r101->hurtNumPos[0] == 0);
    CHECK(r101->isPassRoom == -1);

    const auto* r61320 = find<battle::RoleDef>(61320);
    REQUIRE(r61320 != nullptr);
    CHECK(r61320->roleType == 4);
    CHECK(r61320->radius == 30);
    CHECK(r61320->rigidity == 24);
    CHECK(r61320->hitStiffTime == 0);
    CHECK(r61320->hitDisplacementId == 114320);
    CHECK(r61320->deathDisplacementId == 133);
    CHECK(r61320->deathEffectId == 8);
    CHECK(r61320->koEffectId == 10045);
    CHECK(r61320->monsterCamps == 8);
    CHECK(r61320->velocity == "0.2"_fx);
    CHECK(r61320->weight == "0.1"_fx);
    REQUIRE(r61320->hurtNumPos.size() == 2);
    CHECK(r61320->hurtNumPos[0] == -150);
    CHECK(r61320->hurtNumPos[1] == 100);
    CHECK(r61320->isPassRoom == -1);
}

template <class Fn>
void eachBuffOp(const battle::BuffDef& def, Fn&& fn)
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

void notePredicate(const battle::Predicate& pred, std::array<bool, 14>& seen)
{
    const auto kind = static_cast<int32_t>(pred.m_kind);
    if (kind > 0 && kind < static_cast<int32_t>(seen.size()))
        seen[static_cast<size_t>(kind)] = true;
}

TEST_CASE("battle lowering: every buff row is lowered and ops cover the op set")
{
    const auto& buffs = loadedConfig().table<battle::BuffDef>();
    const auto& source = loadedConfig().table<BuffConfig>();
    CHECK(buffs.size() == source.size());

    std::array<int32_t, 3> statusCount{};
    std::array<bool, 29> opSeen{};
    std::array<bool, 19> valueSeen{};
    std::array<bool, 14> predSeen{};
    std::array<bool, 6> lifeSeen{};
    std::array<bool, 4> activationSeen{};

    for (const auto& def : buffs)
    {
        const auto status = static_cast<int32_t>(def.m_status);
        REQUIRE(status >= 0);
        REQUIRE(status < 3);
        ++statusCount[static_cast<size_t>(status)];

        const int32_t times = def.m_lifecycle.m_times;
        const Fixed interval = def.m_lifecycle.m_intervalSec;
        if (times == -1 && interval > Fixed{})
            lifeSeen[0] = true;
        else if (times == -1 && interval == Fixed{})
            lifeSeen[1] = true;
        else if (times == 0 && interval > Fixed{})
            lifeSeen[2] = true;
        else if (times > 0 && interval > Fixed{})
            lifeSeen[3] = true;
        else if (times > 0 && interval == Fixed{})
            lifeSeen[4] = true;
        else if (times < -1 || interval < Fixed{})
            lifeSeen[5] = true;

        const bool began = def.m_activation.m_beginOn != battle::TriggerPoint::None;
        const bool ended = def.m_activation.m_endOn != battle::TriggerPoint::None;
        if (!began && !ended)
            activationSeen[0] = true;
        else if (began && ended)
            activationSeen[1] = true;
        else if (began)
            activationSeen[2] = true;
        else
            activationSeen[3] = true;

        const auto noteOp = [&](const battle::BuffOp& op) {
            const auto kind = static_cast<int32_t>(op.m_kind);
            if (kind > 0 && kind < static_cast<int32_t>(opSeen.size()))
                opSeen[static_cast<size_t>(kind)] = true;
            const auto value = static_cast<int32_t>(op.m_value.m_kind);
            if (value > 0 && value < static_cast<int32_t>(valueSeen.size()))
                valueSeen[static_cast<size_t>(value)] = true;
            notePredicate(op.m_when, predSeen);
            for (const auto& pred : op.m_predicates)
                notePredicate(pred, predSeen);
        };
        eachBuffOp(def, noteOp);
        for (const auto& pred : def.m_activation.m_conditions)
            notePredicate(pred, predSeen);
    }

    for (const auto& ability : loadedConfig().table<battle::AbilityDef>())
    {
        for (const auto& op : ability.m_ops)
        {
            const auto kind = static_cast<int32_t>(op.m_kind);
            if (kind > 0 && kind < static_cast<int32_t>(opSeen.size()))
                opSeen[static_cast<size_t>(kind)] = true;
            const auto value = static_cast<int32_t>(op.m_value.m_kind);
            if (value > 0 && value < static_cast<int32_t>(valueSeen.size()))
                valueSeen[static_cast<size_t>(value)] = true;
        }
        for (const auto& pred : ability.m_conditions)
            notePredicate(pred, predSeen);
    }

    CHECK(statusCount[0] + statusCount[1] + statusCount[2] == static_cast<int32_t>(buffs.size()));
    CHECK(statusCount[1] > 0);
    CHECK(statusCount[2] > 0);
    for (size_t kind = 1; kind < opSeen.size(); ++kind)
        CHECK_MESSAGE(opSeen[kind], "missing op kind " << kind);
    for (size_t kind = 1; kind < valueSeen.size(); ++kind)
        CHECK_MESSAGE(valueSeen[kind], "missing value kind " << kind);
    for (size_t kind = 1; kind < predSeen.size(); ++kind)
        CHECK_MESSAGE(predSeen[kind], "missing predicate kind " << kind);
    // 源表没有 times<-1 或 interval<0 的行，§5.2 最后一行（不计时）无样本。
    for (size_t shape = 0; shape < lifeSeen.size() - 1; ++shape)
        CHECK_MESSAGE(lifeSeen[shape], "missing lifecycle shape " << shape);
    CHECK_FALSE(lifeSeen[5]);
    for (size_t mode = 0; mode < activationSeen.size(); ++mode)
        CHECK_MESSAGE(activationSeen[mode], "missing activation mode " << mode);
}

TEST_CASE("battle lowering: super armor 101/102 and invincible 200")
{
    for (const int32_t id : {101, 102})
    {
        const auto* buff = find<battle::BuffDef>(id);
        REQUIRE(buff != nullptr);
        CHECK(buff->m_admission.m_ruleId == 1);
        CHECK(buff->m_status == battle::LowerStatus::Lowered);
        CHECK(buff->m_activation.m_beginOn == battle::TriggerPoint::None);
        CHECK(buff->m_activation.m_endOn == battle::TriggerPoint::None);
        CHECK(buff->m_activation.m_latchOn == static_cast<int32_t>(battle::BuffHook::Enter));
        CHECK(buff->m_activation.m_releaseOn == static_cast<int32_t>(battle::BuffHook::Exit));
        CHECK(buff->m_lifecycle.m_executeType == 1);
        CHECK(buff->m_lifecycle.m_times == 0);
        CHECK(buff->m_lifecycle.m_intervalSec == Fixed{});
        bool armor = false;
        bool outline = false;
        eachBuffOp(*buff, [&](const battle::BuffOp& op) {
            if (op.m_kind == battle::BuffOpKind::Flag && op.m_flag == battle::BuffFlag::SuperArmor)
                armor = true;
            if (op.m_kind == battle::BuffOpKind::Visual && op.m_visual == battle::VisualKind::SuperArmorOutline)
                outline = true;
        });
        CHECK(armor);
        CHECK(outline);
    }

    const auto* invincible = find<battle::BuffDef>(200);
    REQUIRE(invincible != nullptr);
    CHECK(invincible->m_admission.m_ruleId == 3);
    bool flag = false;
    eachBuffOp(*invincible, [&](const battle::BuffOp& op) {
        if (op.m_kind == battle::BuffOpKind::Flag && op.m_flag == battle::BuffFlag::Invincible)
            flag = true;
    });
    CHECK(flag);
}

TEST_CASE("battle lowering: special abilities spawn an effect and apply a buff")
{
    const auto* ability = find<battle::AbilityDef>(1);
    REQUIRE(ability != nullptr);
    CHECK(ability->m_status == battle::LowerStatus::Lowered);
    REQUIRE(ability->m_events.size() == 1);
    CHECK(ability->m_events[0] == battle::TriggerPoint::AfterHit);
    CHECK(ability->m_endedType == 0);
    CHECK(ability->m_pvpSetting == 1);
    CHECK(ability->m_intervalSec == Fixed::fromInt(-1));
    REQUIRE(ability->m_ops.size() == 1);
    CHECK(ability->m_ops[0].m_kind == battle::BuffOpKind::SpawnEffect);
    REQUIRE(ability->m_ops[0].m_ids.size() == 1);
    CHECK(ability->m_ops[0].m_ids[0] == 883020);
    CHECK(ability->m_ops[0].m_i0 == 1);
    CHECK(ability->m_ops[0].m_effect == battle::OpEffect::Active);

    const battle::AbilityDef* apply = nullptr;
    for (const auto& row : loadedConfig().table<battle::AbilityDef>())
    {
        for (const auto& op : row.m_ops)
            if (op.m_kind == battle::BuffOpKind::Apply && !op.m_ids.empty())
                apply = &row;
        if (apply != nullptr)
            break;
    }
    REQUIRE(apply != nullptr);
    CHECK(apply->m_id > 0);
}

TEST_CASE("battle lowering: report lists noop and unsupported buffs")
{
    const std::string path = std::string(MUGEN_CONTENT_DIR) + "/mugen/config/lower_report.txt";
    std::ifstream in(path);
    REQUIRE(in.good());
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    CHECK(text.find("battle_buff summary") != std::string::npos);
    CHECK(text.find("unsupported") != std::string::npos);
    CHECK(text.find("22120") != std::string::npos);
    CHECK(text.find("22130") != std::string::npos);
    CHECK(text.find("BuffModifyHPBasedByLostHp") != std::string::npos);
    CHECK(text.find("buff_condition 110 [HasRule]") != std::string::npos);
    CHECK(text.find("HasRule") != std::string::npos);
}
