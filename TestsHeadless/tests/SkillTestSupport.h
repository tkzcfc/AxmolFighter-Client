#pragma once
#include "../support/ContentVfs.h"
#include "doctest.h"
#include "mugen/sim/World.h"
#include "mugen/sim/skill/SkillCaster.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/core/serialize/BinaryArchive.h"

namespace skill_test
{
using namespace mugen;
using namespace mugen::sim;
inline void loadConfig()
{
    REQUIRE(mugen::test::ensureContentVfs());
    REQUIRE(conf::Config::instance().load("/mugen/config/config.bin"));
}
struct ConfigOverride
{
    explicit ConfigOverride(const std::vector<conf::SkillAttackConfig>& skills)
    {
        conf::ConfigArchive archive;
#define MG_COPY_SKILL_TEST_TABLE(kind, Type, Key) archive.kind = conf::Config::instance().table<conf::Type>();
        MG_CONFIG_TABLES(MG_COPY_SKILL_TEST_TABLE)
#undef MG_COPY_SKILL_TEST_TABLE
        for (const auto& skill : skills)
            for (auto& row : archive.skill)
                if (row.id == skill.id) row = skill;
        ByteBuffer bytes;
        for (const auto c : conf::kConfigMagic) bytes.writeByte(c);
        bytes.writeUint32(conf::kConfigVersion);
        bytes.writeUint64(conf::configSchemaHash());
        BinaryWriter writer(bytes);
        writer.write(archive);
        bytes.writeFinish();
        REQUIRE(conf::Config::instance().loadBytes({bytes.getBuffer(), bytes.getBuffer() + bytes.getSize()}));
    }
    ~ConfigOverride()
    {
        (void)(mugen::test::ensureContentVfs() && conf::Config::instance().load("/mugen/config/config.bin"));
    }
};
template<class Predicate>
int32_t findSkill(Predicate predicate)
{
    const auto& rows = conf::Config::instance().table<conf::SkillAttackConfig>();
    const auto it = std::find_if(rows.begin(), rows.end(), predicate);
    REQUIRE(it != rows.end());
    return it->id;
}
inline Entity actor(World& world)
{
    world.init({});
    const auto entity = world.create(GroupKind::Role);
    world.ecs().add(entity, RoleIdentity{});
    world.ecs().add(entity, RoleStatus{});
    world.ecs().add(entity, sim::ExtraState{});
    world.ecs().add(entity, Transform{});
    Vitals vitals;
    vitals.hp = vitals.maxHp = "1000"_fx;
    vitals.mp = vitals.maxMp = "1000000"_fx;
    vitals.ep = vitals.maxEp = "1000000"_fx;
    world.ecs().add(entity, vitals);
    return entity;
}
inline void slots(World& world, Entity entity, std::initializer_list<int32_t> ids)
{
    skill::SkillLoadout data(5);
    data[4].resize(3);
    for (const auto id : ids) data[0].push_back({{{id, -1}}});
    skill::SkillCaster(world, entity).loadSkill(data);
    world.ecs().get<SkillPool>(entity).m_crystals = 1000000;
}
inline void idle(World& world, Entity entity)
{
    skill::SkillCaster(world, entity).resetSkill();
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Idle;
}
}
