#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

// 顿帧期间的受击转移与施法结束回待机的时序单测（原版 dealWithBeHitBegan 先 setFreezeTime 再 dealWithStatus(Hit)）。
namespace
{
// 挂载 VFS 并加载配置（幂等）。
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

// 构造战斗世界（职业 1 / 角色 101，等级 1000，关 AI）。
World makeWorld()
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = 2071501;
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
    setup.m_aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());
    return world;
}

// 取得主控玩家实体。
Entity hero(World& world)
{
    const auto& slots = world.ecs().singleton<PlayerSlots>().m_entities;
    return slots.empty() ? Entity{} : slots.begin()->second;
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
}  // namespace

// 顿帧期间命中：状态同帧转移到 Hit，但 enter（动画/位移）推迟到解冻后补做。
TEST_CASE("freeze: hit during hit-stop still enters Hit, enter deferred until thaw")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    auto& pause = world.ecs().get<PauseComponent>(h);
    pause.m_freezeMs = Fixed::fromInt(1000);
    pause.m_paused   = true;
    auto& hit = world.ecs().get<HitStateComponent>(h);
    hit.m_pendingHitDefId       = 1;
    hit.m_pendingHitType        = 0;
    hit.m_pendingDisplacementId = -1;
    hit.m_pendingTowardX        = 1;

    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    auto& state  = world.ecs().get<RoleStateComponent>(h);
    auto& motion = world.ecs().get<MotionStateComponent>(h);
    // 顿帧未解除：状态已转移，enter 被推迟。
    CHECK(state.m_state == RoleStateId::Hit);
    CHECK(hit.m_enterPending);
    CHECK(motion.m_motionName != "hit");

    // 解冻后补做 enter（播放受击动作）。
    pause.m_freezeMs = {};
    pause.m_paused   = false;
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
    CHECK_FALSE(hit.m_enterPending);
    CHECK(motion.m_motionName == "hit");
}

// 施法结束当帧回到待机（不再停在 Attack，也不再被下一招覆盖跳过 Idle）。
TEST_CASE("cast end: role returns to Idle the same frame the cast finishes")
{
    World world   = makeWorld();
    const Entity h = hero(world);
    REQUIRE(h.value != 0);
    world.tick(cmdInput(1, ControlEvent::CTR_ATKA, 1));
    world.tick(cmdInput(2, ControlEvent::CTR_ATKA, 2));

    bool sawIdleAfterCastEnd = false;
    for (uint64_t frame = 3; frame <= 240; ++frame)
    {
        world.tick(FrameInput{frame, {}, {}, {}});
        const auto& cast  = world.ecs().get<SkillCastComponent>(h);
        const auto& state = world.ecs().get<RoleStateComponent>(h);
        if (cast.m_skill == kNoSkill)
        {
            // 施法结束当帧即回到待机（旧实现攻击分支结束即清 Attack 位）。
            sawIdleAfterCastEnd = state.m_state == RoleStateId::Idle;
            break;
        }
    }
    CHECK(sawIdleAfterCastEnd);
}
