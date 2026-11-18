#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

using namespace mugen;
using namespace mugen::sim;

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
}  // namespace

TEST_CASE("Room 20002 portals keep an empty motion file and a radius damage box")
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = 20002;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(setup.roomId);
    REQUIRE(room != nullptr);
    setup.mapKey = room->mapKey;
    PlayerSetup player;
    player.roleId = type_conversions::toRoleConfigId(CharacterClass::kSwordman);
    player.level  = 1000;
    player.camp   = 2;
    player.slot   = 0;
    player.isAi   = false;
    setup.players.push_back(player);
    setup.aiEnabled = false;
    REQUIRE_FALSE(bootstrapBattle(world, setup).empty());

    for (int i = 0; i < 30; ++i)
        world.tick(FrameInput{world.frame() + 1, {}, {}, {}});

    int32_t portals = 0;
    world.ecs().each<PortalStateComponent>([&](Entity entity, const PortalStateComponent&) {
        ++portals;
        CHECK(world.ecs().get<MotionStateComponent>(entity).motionFile.empty());
        CHECK_FALSE(world.ecs().get<HitboxesComponent>(entity).damage.empty());
    });
    REQUIRE(portals > 0);
}
