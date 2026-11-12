#include "doctest.h"
#include "../support/ContentVfs.h"
#include "BuffTestSupport.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/battle/EffectDef.h"
#include "mugen/conf/battle/BuffDef.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

namespace
{
void loadAssets()
{
    REQUIRE(test::ensureContentVfs());
    auto& config = conf::Config::instance();
    if (!config.loaded())
        REQUIRE(config.load("/mugen/config/config.bin"));
    auto& avatar = *AvatarAssetCache::getInstance();
    if (!avatar.isLoaded())
        REQUIRE(avatar.load("/mugen/config/avatar.bin"));
}

World makeWorld(bool pvp = false)
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = 2071501;
    setup.m_pvp  = pvp;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(2071501);
    REQUIRE(room != nullptr);
    setup.mapKey = room->mapKey;
    PlayerSetup player;
    player.m_roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.m_level  = 1000;
    player.m_camp   = 2;
    player.m_slot   = 0;
    player.m_isAi   = false;
    setup.m_players.push_back(player);
    setup.m_stageId   = 0;
    setup.m_copyId    = 0;
    setup.m_aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.m_configId == configId)
            found = entity;
    });
    return found;
}

FrameInput cmdInput(uint64_t frame, ControlEvent event, uint32_t pressType)
{
    FrameInput input;
    input.frame = frame;
    PlayerCommand command;
    command.controlEvent = event;
    command.quadrant     = ControlQuadrant::None;
    command.pressType    = pressType;
    input.players.push_back({0, {command}});
    return input;
}

int32_t findSlotIndex(const SkillBookComponent& book, std::string_view slot)
{
    for (int32_t i = 0; i < static_cast<int32_t>(book.m_skills.size()); ++i)
    {
        const auto& skill = book.m_skills[static_cast<size_t>(i)];
        if (book.m_slots[static_cast<size_t>(skill.m_slot)] == slot && skill.m_step == 1 && skill.m_link == 1)
            return i;
    }
    return kNoSkill;
}

std::string_view slotName(const SkillBookComponent& book, int32_t index)
{
    const auto& skill = book.m_skills[static_cast<size_t>(index)];
    return book.m_slots[static_cast<size_t>(skill.m_slot)];
}

int32_t ensureSlot(SkillBookComponent& book, std::string_view name, int32_t configId)
{
    int32_t found = findSlotIndex(book, name);
    if (found != kNoSkill)
        return found;
    const int32_t slotIndex = static_cast<int32_t>(book.m_slots.size());
    book.m_slots.emplace_back(name);
    SkillRuntime rt;
    rt.m_slot         = slotIndex;
    rt.m_configId     = configId;
    rt.m_step         = 1;
    rt.m_stepCount    = 1;
    rt.m_link         = 1;
    rt.m_mode         = 1;
    rt.m_releaseMax   = 1;
    rt.m_releaseCount = 1;
    rt.m_finally      = true;
    book.m_skills.push_back(rt);
    return static_cast<int32_t>(book.m_skills.size() - 1);
}
}  // namespace

TEST_CASE("skill crazy rejects E and remaps to slot+")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    auto& cast        = world.ecs().get<SkillCastComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    const int32_t plus = ensureSlot(book, "A+", book.m_skills[static_cast<size_t>(a)].m_configId);

    cast.m_crazy = true;
    CHECK_FALSE(skill::rules::dealButton(world, hero, ControlEvent::CTR_ATKE));
    CHECK(skill::rules::dealButton(world, hero, ControlEvent::CTR_ATKA));
    CHECK(world.ecs().get<SkillCastComponent>(hero).m_skill == plus);
}

TEST_CASE("skill crazy does not spend MP or EP")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    int32_t index     = kNoSkill;
    const conf::battle::SkillDef* def = nullptr;
    for (int32_t i = 0; i < static_cast<int32_t>(book.m_skills.size()); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::battle::SkillDef>(book.m_skills[static_cast<size_t>(i)].m_configId);
        if (row && (row->costMp > 0 || row->costEp > 0))
        {
            index = i;
            def   = row;
            break;
        }
    }
    REQUIRE(index != kNoSkill);
    REQUIRE(def != nullptr);
    auto& health = world.ecs().get<HealthComponent>(hero);
    health.m_mp  = Fixed::fromInt(100);
    health.m_ep  = Fixed::fromInt(1000);
    world.ecs().get<SkillCastComponent>(hero).m_crazy = true;
    skill::rules::beginCast(world, hero, index);
    CHECK(health.m_mp == Fixed::fromInt(100));
    CHECK(health.m_ep == Fixed::fromInt(1000));
}

TEST_CASE("skill beginCast uses pvpCd and syncs other-mode cooldown")
{
    World world       = makeWorld(true);
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    auto extra          = book.m_skills[static_cast<size_t>(a)];
    extra.m_mode        = 2;
    extra.m_releaseMax  = 2;
    extra.m_releaseCount = 2;
    extra.m_cd          = {};
    extra.m_timing      = false;
    book.m_skills.push_back(extra);
    const int32_t other = static_cast<int32_t>(book.m_skills.size() - 1);

    const auto* def = conf::Config::instance().find<conf::battle::SkillDef>(book.m_skills[static_cast<size_t>(a)].m_configId);
    REQUIRE(def != nullptr);
    skill::rules::beginCast(world, hero, a);
    CHECK(book.m_skills[static_cast<size_t>(a)].m_coldMaxTime == Fixed::fromInt(def->pvpCdMs));
    CHECK(book.m_skills[static_cast<size_t>(other)].m_releaseCount == 1);
    CHECK(book.m_skills[static_cast<size_t>(other)].m_coldMaxTime == book.m_skills[static_cast<size_t>(a)].m_coldMaxTime);
}

TEST_CASE("skill isAllowCast gates hit L silence disable crazy and P")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    const int32_t sampleId = book.m_skills[static_cast<size_t>(a)].m_configId;
    const int32_t l        = ensureSlot(book, "L", sampleId);
    const int32_t e        = ensureSlot(book, "E", sampleId);
    const int32_t p        = ensureSlot(book, "P", sampleId);
    auto& state            = world.ecs().get<RoleStateComponent>(hero);
    auto& flags            = world.ecs().get<StatusFlagsComponent>(hero);
    auto& cast             = world.ecs().get<SkillCastComponent>(hero);
    auto& rage             = world.ecs().get<RageStateComponent>(hero);

    state.m_state = RoleStateId::Hit;
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, a));
    CHECK(skill::rules::isAllowCast(world, hero, l));
    state.m_state = RoleStateId::GetUp;
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, l));
    state.m_state = RoleStateId::Idle;
    CHECK(skill::rules::isAllowCast(world, hero, a));

    flags.add(mugen::ExtraState::Silence);
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, a));
    flags.remove(mugen::ExtraState::Silence);

    SkillMod disable;
    disable.m_field = static_cast<int32_t>(conf::battle::SkillModField::DisableSlot);
    disable.m_slot  = book.m_skills[static_cast<size_t>(a)].m_slot;
    disable.m_value = Fixed::fromInt(1);
    book.m_mods.push_back(disable);
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, a));
    book.m_mods.clear();
    CHECK(skill::rules::isAllowCast(world, hero, a));

    cast.m_crazy = true;
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, e));
    cast.m_crazy = false;

    rage.m_rage = {};
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, p));
    rage.m_rage = conf::kPartnerRageMaxAnger;
    CHECK(skill::rules::isAllowCast(world, hero, p));
    rage.m_transformed = true;
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, p));
}

TEST_CASE("skill L break leaves Hit into Attack")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    const int32_t l   = ensureSlot(book, "L", book.m_skills[static_cast<size_t>(a)].m_configId);
    (void)l;
    auto& state                 = world.ecs().get<RoleStateComponent>(hero);
    auto& hit                   = world.ecs().get<HitStateComponent>(hero);
    state.m_state               = RoleStateId::Hit;
    hit.m_pendingHitDefId       = 0;
    hit.m_animationEnd          = false;
    world.tick(cmdInput(1, ControlEvent::CTR_ATKL, 1));
    CHECK(world.ecs().get<RoleStateComponent>(hero).m_state == RoleStateId::Attack);
    const auto& cast = world.ecs().get<SkillCastComponent>(hero);
    REQUIRE(cast.m_skill != kNoSkill);
    CHECK(slotName(world.ecs().get<SkillBookComponent>(hero), cast.m_skill) == "L");
}

TEST_CASE("skill F dodge evades without invincible gate")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    const int32_t f   = ensureSlot(book, "F", book.m_skills[static_cast<size_t>(a)].m_configId);
    world.ecs().get<SkillCastComponent>(hero).m_skill = f;
    CHECK(skill::rules::isDodging(world, hero));

    int32_t effectId = 0;
    const auto& effects = conf::Config::instance().table<conf::battle::EffectDef>();
    REQUIRE_FALSE(effects.empty());
    for (const auto& row : effects)
    {
        if (row.id > 0 && row.hitId > 0)
        {
            effectId = row.id;
            break;
        }
    }
    REQUIRE(effectId > 0);

    const Entity attacker = buff_test::spawnActor(world, 1, 8, 2);
    const Entity effect   = world.ecs().create();
    IdentityComponent effectIdn;
    effectIdn.m_kind     = SpawnKind::Effect;
    effectIdn.m_configId = effectId;
    effectIdn.m_camp     = 8;
    world.ecs().add(effect, effectIdn);
    EffectInfoComponent info;
    info.m_configId = effectId;
    info.m_owner    = attacker;
    world.ecs().add(effect, info);
    world.ecs().add(effect, HitTrackerComponent{});

    ContactEvent contact;
    contact.m_a     = effect;
    contact.m_b     = hero;
    contact.m_phase = ContactPhase::Began;
    world.ecs().singleton<EventQueue<ContactEvent>>().push(contact);
    runCombat(world);

    bool evaded = false;
    for (const auto& event : world.ecs().singleton<EventQueue<HitEvent>>().m_items)
        if (event.m_target == hero && event.m_result == HitResult::Evade)
            evaded = true;
    CHECK(evaded);
}

TEST_CASE("skill partner rage pending grant and P transform")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    int32_t rageSkill = kNoSkill;
    for (int32_t i = 0; i < static_cast<int32_t>(book.m_skills.size()); ++i)
    {
        const auto* def =
            conf::Config::instance().find<conf::battle::SkillDef>(book.m_skills[static_cast<size_t>(i)].m_configId);
        if (def && def->costRage > 0)
        {
            rageSkill = i;
            break;
        }
    }
    if (rageSkill == kNoSkill)
    {
        auto extra     = book.m_skills[static_cast<size_t>(a)];
        extra.m_configId = book.m_skills[static_cast<size_t>(a)].m_configId;
        book.m_skills.push_back(extra);
        rageSkill = static_cast<int32_t>(book.m_skills.size() - 1);
    }
    skill::rules::beginCast(world, hero, rageSkill);
    auto& cast = world.ecs().get<SkillCastComponent>(hero);
    const auto* def =
        conf::Config::instance().find<conf::battle::SkillDef>(book.m_skills[static_cast<size_t>(rageSkill)].m_configId);
    REQUIRE(def != nullptr);
    if (def->costRage > 0)
    {
        CHECK(cast.m_partnerRagePendingGainActive);
        CHECK(cast.m_partnerRagePendingGain == Fixed::fromInt(def->costRage));
        skill::rules::grantPartnerRageOnHit(world, hero);
        CHECK(world.ecs().get<RageStateComponent>(hero).m_rage == Fixed::fromInt(def->costRage));
        CHECK_FALSE(cast.m_partnerRagePendingGainActive);
    }

    const int32_t p = ensureSlot(book, "P", book.m_skills[static_cast<size_t>(a)].m_configId);
    auto& rage      = world.ecs().get<RageStateComponent>(hero);
    rage.m_rage     = conf::kPartnerRageMaxAnger;
    rage.m_maxFreezeMs = Fixed::fromInt(1000);
    skill::rules::beginCast(world, hero, p);
    skill::rules::endCast(world, hero);
    CHECK(world.ecs().get<RageStateComponent>(hero).m_transformed);
    CHECK(world.ecs().get<RageStateComponent>(hero).m_freezeMs == Fixed::fromInt(1000));
}
