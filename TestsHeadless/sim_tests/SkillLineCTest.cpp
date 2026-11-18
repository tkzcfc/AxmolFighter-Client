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
    setup.pvp  = pvp;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(2071501);
    REQUIRE(room != nullptr);
    setup.mapKey = room->mapKey;
    PlayerSetup player;
    player.roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.level  = 1000;
    player.camp   = 2;
    player.slot   = 0;
    player.isAi   = false;
    setup.players.push_back(player);
    setup.stageId   = 0;
    setup.copyId    = 0;
    setup.aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

Entity roleByConfig(World& world, int32_t configId)
{
    Entity found;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.configId == configId)
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
    for (int32_t i = 0; i < static_cast<int32_t>(book.skills.size()); ++i)
    {
        const auto& skill = book.skills[static_cast<size_t>(i)];
        if (book.slots[static_cast<size_t>(skill.slot)] == slot && skill.step == 1 && skill.link == 1)
            return i;
    }
    return kNoSkill;
}

std::string_view slotName(const SkillBookComponent& book, int32_t index)
{
    const auto& skill = book.skills[static_cast<size_t>(index)];
    return book.slots[static_cast<size_t>(skill.slot)];
}

int32_t ensureSlot(SkillBookComponent& book, std::string_view name, int32_t configId)
{
    int32_t found = findSlotIndex(book, name);
    if (found != kNoSkill)
        return found;
    const int32_t slotIndex = static_cast<int32_t>(book.slots.size());
    book.slots.emplace_back(name);
    SkillRuntime rt;
    rt.slot         = slotIndex;
    rt.configId     = configId;
    rt.step         = 1;
    rt.stepCount    = 1;
    rt.link         = 1;
    rt.mode         = 1;
    rt.releaseMax   = 1;
    rt.releaseCount = 1;
    rt.finally      = true;
    book.skills.push_back(rt);
    return static_cast<int32_t>(book.skills.size() - 1);
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
    const int32_t plus = ensureSlot(book, "A+", book.skills[static_cast<size_t>(a)].configId);

    cast.crazy = true;
    CHECK_FALSE(skill::rules::dealButton(world, hero, ControlEvent::CTR_ATKE));
    CHECK(skill::rules::dealButton(world, hero, ControlEvent::CTR_ATKA));
    CHECK(world.ecs().get<SkillCastComponent>(hero).skill == plus);
}

TEST_CASE("skill crazy does not spend MP or EP")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    int32_t index     = kNoSkill;
    const conf::battle::SkillDef* def = nullptr;
    for (int32_t i = 0; i < static_cast<int32_t>(book.skills.size()); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::battle::SkillDef>(book.skills[static_cast<size_t>(i)].configId);
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
    health.mp  = Fixed::fromInt(100);
    health.ep  = Fixed::fromInt(1000);
    world.ecs().get<SkillCastComponent>(hero).crazy = true;
    skill::rules::beginCast(world, hero, index);
    CHECK(health.mp == Fixed::fromInt(100));
    CHECK(health.ep == Fixed::fromInt(1000));
}

TEST_CASE("skill beginCast uses pvpCd and syncs other-mode cooldown")
{
    World world       = makeWorld(true);
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    auto extra          = book.skills[static_cast<size_t>(a)];
    extra.mode        = 2;
    extra.releaseMax  = 2;
    extra.releaseCount = 2;
    extra.cd          = {};
    extra.timing      = false;
    book.skills.push_back(extra);
    const int32_t other = static_cast<int32_t>(book.skills.size() - 1);

    const auto* def = conf::Config::instance().find<conf::battle::SkillDef>(book.skills[static_cast<size_t>(a)].configId);
    REQUIRE(def != nullptr);
    skill::rules::beginCast(world, hero, a);
    CHECK(book.skills[static_cast<size_t>(a)].coldMaxTime == Fixed::fromInt(def->pvpCdMs));
    CHECK(book.skills[static_cast<size_t>(other)].releaseCount == 1);
    CHECK(book.skills[static_cast<size_t>(other)].coldMaxTime == book.skills[static_cast<size_t>(a)].coldMaxTime);
}

TEST_CASE("skill isAllowCast gates hit L silence disable crazy and P")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    const int32_t sampleId = book.skills[static_cast<size_t>(a)].configId;
    const int32_t l        = ensureSlot(book, "L", sampleId);
    const int32_t e        = ensureSlot(book, "E", sampleId);
    const int32_t p        = ensureSlot(book, "P", sampleId);
    auto& state            = world.ecs().get<RoleStateComponent>(hero);
    auto& flags            = world.ecs().get<StatusFlagsComponent>(hero);
    auto& cast             = world.ecs().get<SkillCastComponent>(hero);
    auto& rage             = world.ecs().get<RageStateComponent>(hero);

    state.state = RoleStateId::Hit;
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, a));
    CHECK(skill::rules::isAllowCast(world, hero, l));
    state.state = RoleStateId::GetUp;
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, l));
    state.state = RoleStateId::Idle;
    CHECK(skill::rules::isAllowCast(world, hero, a));

    flags.add(mugen::ExtraState::Silence);
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, a));
    flags.remove(mugen::ExtraState::Silence);

    SkillMod disable;
    disable.field = static_cast<int32_t>(conf::battle::SkillModField::DisableSlot);
    disable.slot  = book.skills[static_cast<size_t>(a)].slot;
    disable.value = Fixed::fromInt(1);
    book.mods.push_back(disable);
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, a));
    book.mods.clear();
    CHECK(skill::rules::isAllowCast(world, hero, a));

    cast.crazy = true;
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, e));
    cast.crazy = false;

    rage.rage = {};
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, p));
    rage.rage = conf::kPartnerRageMaxAnger;
    CHECK(skill::rules::isAllowCast(world, hero, p));
    rage.transformed = true;
    CHECK_FALSE(skill::rules::isAllowCast(world, hero, p));
}

TEST_CASE("skill L break leaves Hit into Attack")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    const int32_t l   = ensureSlot(book, "L", book.skills[static_cast<size_t>(a)].configId);
    (void)l;
    auto& state                 = world.ecs().get<RoleStateComponent>(hero);
    auto& hit                   = world.ecs().get<HitStateComponent>(hero);
    state.state               = RoleStateId::Hit;
    hit.pendingHitDefId       = 0;
    hit.animationEnd          = false;
    world.tick(cmdInput(1, ControlEvent::CTR_ATKL, 1));
    CHECK(world.ecs().get<RoleStateComponent>(hero).state == RoleStateId::Attack);
    const auto& cast = world.ecs().get<SkillCastComponent>(hero);
    REQUIRE(cast.skill != kNoSkill);
    CHECK(slotName(world.ecs().get<SkillBookComponent>(hero), cast.skill) == "L");
}

TEST_CASE("skill F dodge evades without invincible gate")
{
    World world       = makeWorld();
    const Entity hero = roleByConfig(world, 101);
    auto& book        = world.ecs().get<SkillBookComponent>(hero);
    const int32_t a   = findSlotIndex(book, "A");
    REQUIRE(a != kNoSkill);
    const int32_t f   = ensureSlot(book, "F", book.skills[static_cast<size_t>(a)].configId);
    world.ecs().get<SkillCastComponent>(hero).skill = f;
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
    effectIdn.kind     = SpawnKind::Effect;
    effectIdn.configId = effectId;
    effectIdn.camp     = 8;
    world.ecs().add(effect, effectIdn);
    EffectInfoComponent info;
    info.configId = effectId;
    info.owner    = attacker;
    world.ecs().add(effect, info);
    world.ecs().add(effect, HitTrackerComponent{});

    ContactEvent contact;
    contact.a     = effect;
    contact.b     = hero;
    contact.phase = ContactPhase::Began;
    world.ecs().singleton<EventQueue<ContactEvent>>().push(contact);
    runCombat(world);

    bool evaded = false;
    for (const auto& event : world.ecs().singleton<EventQueue<HitEvent>>().items)
        if (event.target == hero && event.result == HitResult::Evade)
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
    for (int32_t i = 0; i < static_cast<int32_t>(book.skills.size()); ++i)
    {
        const auto* def =
            conf::Config::instance().find<conf::battle::SkillDef>(book.skills[static_cast<size_t>(i)].configId);
        if (def && def->costRage > 0)
        {
            rageSkill = i;
            break;
        }
    }
    if (rageSkill == kNoSkill)
    {
        auto extra     = book.skills[static_cast<size_t>(a)];
        extra.configId = book.skills[static_cast<size_t>(a)].configId;
        book.skills.push_back(extra);
        rageSkill = static_cast<int32_t>(book.skills.size() - 1);
    }
    skill::rules::beginCast(world, hero, rageSkill);
    auto& cast = world.ecs().get<SkillCastComponent>(hero);
    const auto* def =
        conf::Config::instance().find<conf::battle::SkillDef>(book.skills[static_cast<size_t>(rageSkill)].configId);
    REQUIRE(def != nullptr);
    if (def->costRage > 0)
    {
        CHECK(cast.partnerRagePendingGainActive);
        CHECK(cast.partnerRagePendingGain == Fixed::fromInt(def->costRage));
        skill::rules::grantPartnerRageOnHit(world, hero);
        CHECK(world.ecs().get<RageStateComponent>(hero).rage == Fixed::fromInt(def->costRage));
        CHECK_FALSE(cast.partnerRagePendingGainActive);
    }

    const int32_t p = ensureSlot(book, "P", book.skills[static_cast<size_t>(a)].configId);
    auto& rage      = world.ecs().get<RageStateComponent>(hero);
    rage.rage     = conf::kPartnerRageMaxAnger;
    rage.maxFreezeMs = Fixed::fromInt(1000);
    skill::rules::beginCast(world, hero, p);
    skill::rules::endCast(world, hero);
    CHECK(world.ecs().get<RageStateComponent>(hero).transformed);
    CHECK(world.ecs().get<RageStateComponent>(hero).freezeMs == Fixed::fromInt(1000));
}
