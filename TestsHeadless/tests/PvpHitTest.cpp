#include <doctest.h>
#include "../support/ScenarioHarness.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/sim/BattleBootstrap.h"

#include <array>

using namespace mugen;
using namespace mugen::sim;

namespace
{
constexpr int32_t kDuelRoom = 30002;  // 纯 PvP 竞技场。

// 构造与对战服相同的决斗配置：槽位 0 阵营 2（Our），槽位 1 阵营 8（Enemy）；中立阵营 1 的角色不会被特效命中。
WorldSetup duelSetup(CharacterClass first, CharacterClass second)
{
    WorldSetup setup = test::ScenarioHarness::roomSetup(kDuelRoom);
    setup.m_pvp      = true;
    const std::array<CharacterClass, 2> classes{first, second};
    for (uint32_t slot = 0; slot < 2; ++slot)
    {
        PlayerSetup player;
        player.m_slot   = slot;
        player.m_class  = classes[slot];
        player.m_roleId = type_conversions::toRoleConfigId(classes[slot]);
        player.m_level  = 100;
        player.m_camp   = slot == 0 ? 2 : 8;
        player.m_isAi   = false;
        setup.m_players.push_back(player);
    }
    return setup;
}

void tickWith(World& world, std::vector<PlayerInput> players)
{
    FrameInput input;
    input.frame   = world.ecs().singleton<WorldClock>().frame + 1;
    input.players = std::move(players);
    world.tick(input);
}

// 让指定槽位在近距离朝向对手释放普通攻击，返回对手损失的生命值。
Fixed attackDamage(CharacterClass first, CharacterClass second, uint32_t attackerSlot)
{
    World world;
    const auto heroes = bootstrapBattle(world, duelSetup(first, second));
    REQUIRE(heroes.size() == 2);
    auto& ecs = world.ecs();
    for (int i = 0; i < 12; ++i)
        tickWith(world, {});
    REQUIRE(ecs.singleton<sim::RoomState>().m_state == mugen::RoomState::Process);

    const Entity attacker = heroes[attackerSlot];
    const Entity target   = heroes[1 - attackerSlot];
    auto& attackerMove    = ecs.get<Transform>(attacker);
    auto& targetMove      = ecs.get<Transform>(target);
    const int32_t toward  = attackerSlot == 0 ? -1 : 1;
    attackerMove.position = {Fixed::fromInt(1000), {}, Fixed::fromInt(300)};
    targetMove.position   = {Fixed::fromInt(1000 + toward * 80), {}, Fixed::fromInt(300)};
    attackerMove.towardX  = toward;
    attackerMove.vectorX  = Fixed::fromInt(toward);
    targetMove.towardX    = -toward;
    targetMove.vectorX    = Fixed::fromInt(-toward);

    const Fixed before = ecs.get<Vitals>(target).hp;
    PlayerCommand attack;
    attack.controlEvent = ControlEvent::CTR_ATKA;
    attack.pressType    = 1;
    tickWith(world, {PlayerInput{attackerSlot, {attack}}});
    for (int i = 0; i < 90; ++i)
        tickWith(world, {});
    return before - ecs.get<Vitals>(target).hp;
}
}  // namespace

// 槽位 0（阵营 2）与槽位 1（阵营 1）的普通攻击必须双向命中。
TEST_CASE("Pvp duel basic attacks hit in both directions")
{
    const std::array<std::pair<CharacterClass, CharacterClass>, 4> pairs{{
        {CharacterClass::kSwordman, CharacterClass::kSwordman},
        {CharacterClass::kSwordman, CharacterClass::kRanger},
        {CharacterClass::kRanger, CharacterClass::kSwordman},
        {CharacterClass::kRanger, CharacterClass::kMage},
    }};
    for (const auto& [first, second] : pairs)
    {
        CAPTURE(static_cast<int>(first));
        CAPTURE(static_cast<int>(second));
        CHECK_MESSAGE(attackDamage(first, second, 0) > Fixed{}, "camp 2 attacker must damage camp 1 target");
        CHECK_MESSAGE(attackDamage(first, second, 1) > Fixed{}, "camp 1 attacker must damage camp 2 target");
    }
}
