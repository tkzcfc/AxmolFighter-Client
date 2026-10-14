#include "SkillTestSupport.h"
#include "mugen/core/serialize/BinaryArchive.h"
#include "mugen/conf/BattleConst.h"

using namespace skill_test;

namespace
{
struct AiFixture
{
    World m_world;
    Entity m_entity;
    SkillAi m_ai;
    conf::SkillAiConfig m_row;
    AiFixture()
    {
        loadConfig();
        m_row = conf::Config::instance().table<conf::SkillAiConfig>().front();
        m_row.composition = {{1, 6}, {3, 7}};
        m_row.oppDisX = {10, 20};
        m_row.selfHp = {50, 100};
        m_row.oppStatus = 1;
        m_row.selfStatus = 6;
        m_row.prob = 100;
        m_row.checkCd = 100;
        m_entity = actor(m_world);
        m_world.ecs().add(m_entity, SkillPool{});
        const auto target = m_world.create(GroupKind::Role);
        m_world.ecs().add(target, Transform{});
        m_world.ecs().add(target, RoleStatus{EntityRoleStatus::Hit, {}});
        m_world.ecs().get<Transform>(target).position.x = "22"_fx;
        m_world.ecs().get<SkillPool>(m_entity).m_target = target;
        m_ai.m_configId = m_row.id;
        m_ai.m_moveDistanceFrame = "2"_fx;
        m_ai.m_useCount = 2;
    }
    void publish()
    {
        conf::ConfigArchive archive;
        archive.skill_ai.push_back(m_row);
        ByteBuffer bytes;
        for (const auto c : conf::kConfigMagic) bytes.writeByte(c);
        bytes.writeUint32(conf::kConfigVersion);
        bytes.writeUint64(conf::configSchemaHash());
        BinaryWriter writer(bytes);
        writer.write(archive);
        bytes.writeFinish();
        REQUIRE(conf::Config::instance().loadBytes(std::vector<uint8_t>(bytes.getBuffer(), bytes.getBuffer() + bytes.getSize())));
    }
    ~AiFixture()
    {
        conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin");
    }
};
}

TEST_CASE("Skill AI AND OR composition respects one frame inclusive tolerance")
{
    AiFixture f;
    f.publish();
    const auto target = f.m_world.ecs().get<SkillPool>(f.m_entity).m_target;
    SUBCASE("AND and first OR pass at tolerance boundary") { CHECK(skill::SkillCaster(f.m_world, f.m_entity).checkComposition(f.m_ai)); }
    SUBCASE("AND fails just outside tolerance")
    {
        f.m_world.ecs().get<Transform>(target).position.x += Fixed::fromRaw(1);
        CHECK_FALSE(skill::SkillCaster(f.m_world, f.m_entity).checkComposition(f.m_ai));
    }
    SUBCASE("AND fails HP despite passing OR")
    {
        f.m_world.ecs().get<Vitals>(f.m_entity).hp = "499"_fx;
        CHECK_FALSE(skill::SkillCaster(f.m_world, f.m_entity).checkComposition(f.m_ai));
    }
    SUBCASE("second OR passes despite first OR failure")
    {
        f.m_world.ecs().get<RoleStatus>(target).eEntityStatus = EntityRoleStatus::Idle;
        f.m_world.ecs().get<RoleStatus>(f.m_entity).eEntityStatus = EntityRoleStatus::Attack;
        CHECK(skill::SkillCaster(f.m_world, f.m_entity).checkComposition(f.m_ai));
    }
    SUBCASE("all OR fail")
    {
        f.m_world.ecs().get<RoleStatus>(target).eEntityStatus = EntityRoleStatus::Idle;
        CHECK_FALSE(skill::SkillCaster(f.m_world, f.m_entity).checkComposition(f.m_ai));
    }
    SUBCASE("minus one OR unconditional")
    {
        f.m_row.composition[1] = {-1};
        f.publish();
        CHECK(skill::SkillCaster(f.m_world, f.m_entity).checkComposition(f.m_ai));
    }
}

TEST_CASE("Skill AI cooldown probability and use count gates consume RNG only after composition")
{
    AiFixture f;
    f.publish();
    const auto before = f.m_world.hash();
    f.m_ai.m_loadCd = "1"_fx;
    CHECK_FALSE(skill::SkillCaster(f.m_world, f.m_entity).checkAi(f.m_ai));
    CHECK(f.m_world.hash() == before);
    f.m_ai.m_loadCd = {};
    REQUIRE(skill::SkillCaster(f.m_world, f.m_entity).checkAi(f.m_ai));
    CHECK(f.m_ai.m_useCount == 1);
    CHECK(f.m_ai.m_checkCd == "100"_fx);
    CHECK_FALSE(skill::SkillCaster(f.m_world, f.m_entity).checkAi(f.m_ai));
    f.m_ai.m_checkCd = {};
    f.m_row.prob = 0;
    f.publish();
    CHECK_FALSE(skill::SkillCaster(f.m_world, f.m_entity).checkAi(f.m_ai));
    CHECK(f.m_ai.m_useCount == 1);
    CHECK(f.m_ai.m_checkCd == "100"_fx);
}

TEST_CASE("Skill crazy EP drain waits for slot change before ending")
{
    World world;
    const auto entity = actor(world);
    world.ecs().add(entity, SkillPool{});
    auto& pool = world.ecs().get<SkillPool>(entity);
    pool.m_crazy = true;
    pool.m_slotChanged = false;
    world.ecs().get<Vitals>(entity).ep = "1"_fx;
    skill::SkillCaster(world, entity).updateCrazy();
    CHECK(world.ecs().get<Vitals>(entity).ep == Fixed{});
    CHECK(pool.m_crazy);
    pool.m_slotChanged = true;
    skill::SkillCaster(world, entity).updateCrazy();
    CHECK_FALSE(pool.m_crazy);
}
