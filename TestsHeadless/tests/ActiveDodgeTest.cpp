#include "SkillTestSupport.h"
#include "mugen/sim/effect/Effect.h"
#include "mugen/sim/hit/Hit.h"
#include <algorithm>

namespace
{
using namespace skill_test;

// 用真实效果行驱动命中，并把目标当前技能装到指定基础槽。
struct DodgeHit
{
    conf::ConfigArchive m_archive;
    World m_world;
    Entity m_hero, m_monster, m_effect;
    int32_t m_effectId = 920000, m_hitId = 920000;

    DodgeHit()
    {
        loadConfig();
#define COPY_DODGE_TABLE(kind, Type, Key) m_archive.kind = conf::Config::instance().table<conf::Type>();
        MG_CONFIG_TABLES(COPY_DODGE_TABLE)
#undef COPY_DODGE_TABLE
        REQUIRE(conf::Config::instance().find<conf::EffectConfig>(m_effectId));
        m_hitId                    = effectRow().hitId;
        auto& hit                  = hitRow();
        hit.hitInterval            = -1;
        hit.hitCounts              = -1;
        hit.hitMust                = 0;
        hit.hitType                = 0;
        hit.hitCondition           = -1;
        auto& effect               = effectRow();
        effect.hitTarget           = 0;
        effect.hitEffectIds        = {-1};
        effect.debuffId            = {-1};
        effect.buffId              = {"-1"_fx};
        effect.buffAllId           = -1;
        effect.hitExtraControl     = {0, 0};
        effect.specialabilityId    = -1;
        effect.specialabilityAllId = -1;
        load();
        m_world.init({});
        m_hero    = role(static_cast<int>(EntityRoleType::kHero), 2);
        m_monster = role(static_cast<int>(EntityRoleType::kMonster), 8);
        m_effect  = effect::create(m_world, m_hero, m_effectId);
        REQUIRE(m_effect.value != 0);
        m_world.ecs().get<EffectState>(m_effect).m_skillId     = 920000;
        m_world.ecs().get<EffectState>(m_effect).m_baseSkillId = 920000;
        auto& attacker                                         = m_world.ecs().get<Attributes>(m_hero);
        auto& target                                           = m_world.ecs().get<Attributes>(m_monster);
        attacker.basic[14]                                     = {};
        attacker.extend[11]                                    = {};
        target.basic[13]                                       = {};
        target.extend[9]                                       = {};
        seedFailingRoll();
    }

    ~DodgeHit() { conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin"); }

    conf::EffectConfig& effectRow()
    {
        auto it = std::find_if(m_archive.effect.begin(), m_archive.effect.end(),
                               [&](const auto& row) { return row.id == m_effectId; });
        REQUIRE(it != m_archive.effect.end());
        return *it;
    }

    conf::SkillHitConfig& hitRow()
    {
        auto it = std::find_if(m_archive.skill_hit.begin(), m_archive.skill_hit.end(),
                               [&](const auto& row) { return row.id == m_hitId; });
        REQUIRE(it != m_archive.skill_hit.end());
        return *it;
    }

    void load()
    {
        ByteBuffer bytes;
        for (const auto c : conf::kConfigMagic)
            bytes.writeByte(c);
        bytes.writeUint32(conf::kConfigVersion);
        bytes.writeUint64(conf::configSchemaHash());
        BinaryWriter(bytes).write(m_archive);
        bytes.writeFinish();
        REQUIRE(conf::Config::instance().loadBytes({bytes.getBuffer(), bytes.getBuffer() + bytes.getSize()}));
    }

    Entity role(int type, int camp)
    {
        const auto& rows = conf::Config::instance().table<conf::RoleConfig>();
        auto it          = std::find_if(rows.begin(), rows.end(),
                                        [&](const auto& row) { return row.roleType == type && row.hitStiffTime == -1; });
        REQUIRE(it != rows.end());
        auto& ecs         = m_world.ecs();
        const auto entity = m_world.create(GroupKind::Role);
        RoleIdentity identity;
        identity.roleId      = it->id;
        identity.roleType    = type;
        identity.camp        = camp;
        identity.hostileCamp = camp == 2 ? 25 : 7;
        ecs.add(entity, identity);
        ecs.add(entity, RoleStatus{});
        ecs.add(entity, sim::ExtraState{});
        ecs.add(entity, Transform{});
        ecs.add(entity, RoleTimers{});
        ecs.add(entity, RoleHitState{});
        ecs.add(entity, Displacement{});
        ecs.add(entity, MotionState{});
        Attributes attributes;
        attributes.basic[4] = "100000"_fx;
        attributes.basic[5] = "800"_fx;
        attributes.basic[6] = "800"_fx;
        attributes.basic[7] = "400"_fx;
        attributes.basic[8] = "200"_fx;
        ecs.add(entity, attributes);
        Vitals vitals;
        vitals.hp = vitals.maxHp = "100000"_fx;
        ecs.add(entity, vitals);
        return entity;
    }

    // 装六个基础槽后把当前技能设到请求的槽位，与生产读取方式一致。
    void arm(std::string_view slot)
    {
        const auto id = findSkill([](const auto&) { return true; });
        skill::SkillLoadout data(5);
        for (int i = 0; i < 6; ++i)
            data[0].push_back({{{id, -1}}});
        skill::loadSkill(m_world, m_monster, data);
        auto& pool       = m_world.ecs().get<SkillPool>(m_monster);
        const auto index = skill::getGivenSkill(pool, slot);
        REQUIRE(index != kNoSkill);
        skill::setSkillBase(pool, index);
        REQUIRE(skill::baseSlot(pool, pool.m_skills.at(pool.m_current)) == slot);
    }

    // 闪避率为零时仍可能抽到零并判定闪避，因此选一个首次抽样失败的种子。
    void seedFailingRoll()
    {
        for (uint64_t seed = 1; seed < 64; ++seed)
        {
            m_world.ecs().singleton<RngState>().random.seed(seed);
            auto clone = m_world.clone();
            hit::DamageInput input;
            REQUIRE(hit::damageInput(clone, m_effect, m_monster, input));
            if (!hit::attributeDodge(input, hit::damageDraw(clone.ecs().singleton<RngState>().random)))
                return;
        }
        REQUIRE(false);
    }

    void strike() { hit::onEffectCollision(m_world, m_effect, m_monster, CollisionEvent::Began, {}); }

    bool tip(std::string_view text) const
    {
        for (const auto& event : m_world.ecs().singleton<PresentationEvents>().events)
            if (event.entity == m_monster)
                if (const auto* item = std::get_if<Tips>(&event.payload))
                    if (item->text == text)
                        return true;
        return false;
    }
};
}  // namespace

// 非无敌状态下，当前技能基础槽 F 走主动闪避：不扣生命，只提示 DODGE。
TEST_CASE("Active Dodge on skill slot F skips damage and emits DODGE")
{
    DodgeHit scene;
    scene.arm("F");
    REQUIRE_FALSE(scene.m_world.ecs().get<sim::ExtraState>(scene.m_monster).active(mugen::ExtraState::Invincible));
    const auto hp = scene.m_world.ecs().get<Vitals>(scene.m_monster).hp;
    scene.strike();
    CHECK(scene.m_world.ecs().get<Vitals>(scene.m_monster).hp == hp);
    CHECK(scene.tip("DODGE"));
    CHECK_FALSE(scene.tip("MISS"));
}

// 非 F 槽且属性闪避抽样失败时仍扣生命，不走主动闪避。
TEST_CASE("Non-F skill slot with a failing attribute Dodge still applies damage")
{
    DodgeHit scene;
    scene.arm("A");
    const auto hp = scene.m_world.ecs().get<Vitals>(scene.m_monster).hp;
    scene.strike();
    CHECK(scene.m_world.ecs().get<Vitals>(scene.m_monster).hp < hp);
    CHECK_FALSE(scene.tip("DODGE"));
    CHECK_FALSE(scene.tip("MISS"));
}
