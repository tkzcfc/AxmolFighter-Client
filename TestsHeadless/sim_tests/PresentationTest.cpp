#include "BuffTestSupport.h"
#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/present/PresentationEvent.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/battle/BuffDef.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include <variant>

using namespace buff_test;
using namespace mugen;
using namespace mugen::sim;
using namespace mugen::conf::battle;

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

// 构造指定房间的战斗世界（职业 1 / 角色 101，等级 1000，关 AI）。
World makeWorld(int32_t roomId)
{
    loadAssets();
    World world;
    WorldSetup setup;
    setup.seed   = 31;
    setup.mode   = WorldMode::Battle;
    setup.roomId = roomId;
    const auto* room = conf::Config::instance().find<conf::RoomConfig>(roomId);
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
    return world;
}

// 推进一帧空输入。
void tickOnce(World& world)
{
    world.tick(FrameInput{world.frame() + 1, {}, {}, {}});
}

// 在队列里查找指定载荷。
template <typename T>
bool hasPayload(const PresentationEvents& queue)
{
    for (const auto& event : queue.events)
        if (std::holds_alternative<T>(event.payload))
            return true;
    return false;
}

template <typename T>
const T* findPayload(const PresentationEvents& queue)
{
    for (const auto& event : queue.events)
        if (const T* payload = std::get_if<T>(&event.payload))
            return payload;
    return nullptr;
}
}  // namespace

TEST_CASE("presentation ordinal resets each frame and seq is monotonic")
{
    Harness h;
    CueEvent white;
    white.entity = h.self;
    white.kind   = CueKind::WhiteFrame;
    white.param  = 1;
    CueEvent stealth;
    stealth.entity = h.self;
    stealth.kind   = CueKind::StealthView;
    stealth.param  = 1;
    h.world.ecs().singleton<EventQueue<CueEvent>>().push(white);
    h.world.ecs().singleton<EventQueue<CueEvent>>().push(stealth);
    h.tickWorld();

    const auto& queue = h.world.ecs().singleton<PresentationEvents>();
    REQUIRE(queue.events.size() >= 2);
    for (size_t i = 0; i < queue.events.size(); ++i)
    {
        CHECK(queue.events[i].ordinal == i);
        CHECK(queue.events[i].frame == h.world.frame());
        if (i > 0)
            CHECK(queue.events[i].seq == queue.events[i - 1].seq + 1);
    }
    CHECK(queue.events.front().seq == 0);
    const uint64_t seqAfterFirst = queue.nextSeq;
    CHECK(seqAfterFirst == queue.events.size());
    CHECK(hasPayload<WhiteFrame>(queue));
    CHECK(hasPayload<StealthView>(queue));

    CueEvent armor;
    armor.entity = h.self;
    armor.kind   = CueKind::SuperArmorOutline;
    armor.param  = 1;
    h.world.ecs().singleton<EventQueue<CueEvent>>().push(armor);
    h.tickWorld();
    REQUIRE_FALSE(queue.events.empty());
    CHECK(queue.events.front().ordinal == 0);
    CHECK(queue.events.front().seq == seqAfterFirst);
    CHECK_FALSE(hasPayload<WhiteFrame>(queue));
    CHECK(hasPayload<SuperArmorBorder>(queue));
}

TEST_CASE("presentation room startup emits RoomStateChanged and Tips")
{
    World world = makeWorld(2010101);
    auto& room  = world.ecs().singleton<sim::RoomState>();
    bool sawState = false;
    bool sawTips  = false;
    for (int i = 0; i < 16; ++i)
    {
        tickOnce(world);
        const auto& queue = world.ecs().singleton<PresentationEvents>();
        sawState |= hasPayload<RoomStateChanged>(queue);
        sawTips |= hasPayload<Tips>(queue);
    }
    CHECK(sawState);
    if (room.config.openTips != -1 || room.config.sceneName > 0)
        CHECK(sawTips);
}

TEST_CASE("presentation hit emits HurtNumber and RoleIntensity")
{
    Harness h;
    HitEvent hit;
    hit.target   = h.self;
    hit.result   = HitResult::Hit;
    hit.damage   = Fixed::fromInt(12);
    hit.critical = true;
    hit.contact  = {Fixed::fromInt(10), Fixed::fromInt(20), Fixed::fromInt(5)};
    h.world.ecs().singleton<EventQueue<HitEvent>>().push(hit);
    StateEvent state;
    state.entity = h.self;
    state.from   = static_cast<int32_t>(RoleStateId::Idle);
    state.to     = static_cast<int32_t>(RoleStateId::Hit);
    h.world.ecs().singleton<EventQueue<StateEvent>>().push(state);
    h.tickWorld();

    const auto& queue = h.world.ecs().singleton<PresentationEvents>();
    const auto* hurt  = findPayload<HurtNumber>(queue);
    REQUIRE(hurt != nullptr);
    CHECK(hurt->value == 12);
    CHECK(hurt->critical);
    CHECK(hurt->hasContact);
    CHECK(hurt->contact.x == Fixed::fromInt(10));
    CHECK(hurt->contact.y == Fixed::fromInt(25));
    const auto* intensity = findPayload<RoleIntensity>(queue);
    REQUIRE(intensity != nullptr);
    CHECK(intensity->value == "1.3"_fx);
}

TEST_CASE("presentation buff spine and visual cues")
{
    Harness h;
    BuffDef def = makeBuff(nextBuffId());
    def.presentation.spineId   = 77;
    def.presentation.spineStep = {1, 1, 1};
    const int32_t id           = h.add(def);
    h.admit(id);
    h.tickWorld();
    const auto* added = findPayload<BuffSpine>(h.world.ecs().singleton<PresentationEvents>());
    REQUIRE(added != nullptr);
    CHECK(added->spineId == 77);
    CHECK(added->stage == 1);
    CHECK(added->buffId == id);

    CueEvent white;
    white.entity = h.self;
    white.kind   = CueKind::WhiteFrame;
    white.param  = 1;
    CueEvent stealth;
    stealth.entity = h.self;
    stealth.kind   = CueKind::StealthView;
    stealth.param  = 0;
    CueEvent armor;
    armor.entity = h.self;
    armor.kind   = CueKind::SuperArmorOutline;
    armor.param  = 1;
    h.world.ecs().singleton<EventQueue<CueEvent>>().push(white);
    h.world.ecs().singleton<EventQueue<CueEvent>>().push(stealth);
    h.world.ecs().singleton<EventQueue<CueEvent>>().push(armor);
    h.remove(id);
    h.tickWorld();
    const auto& queue = h.world.ecs().singleton<PresentationEvents>();
    const auto* removed = findPayload<BuffSpine>(queue);
    REQUIRE(removed != nullptr);
    CHECK(removed->stage == 3);
    const auto* frame = findPayload<WhiteFrame>(queue);
    REQUIRE(frame != nullptr);
    CHECK(frame->enabled);
    const auto* view = findPayload<StealthView>(queue);
    REQUIRE(view != nullptr);
    CHECK_FALSE(view->enabled);
    const auto* border = findPayload<SuperArmorBorder>(queue);
    REQUIRE(border != nullptr);
    CHECK(border->enabled);
}

TEST_CASE("presentation snapshot roundtrips PresentationEvents at version 9")
{
    CHECK(kWorldSnapshotVersion == 9);
    Harness h;
    CueEvent white;
    white.entity = h.self;
    white.kind   = CueKind::WhiteFrame;
    white.param  = 1;
    h.world.ecs().singleton<EventQueue<CueEvent>>().push(white);
    h.tickWorld();
    const auto& src = h.world.ecs().singleton<PresentationEvents>();
    REQUIRE_FALSE(src.events.empty());

    ByteBuffer buffer;
    h.world.serialize(buffer);
    buffer.resetPosition();
    World restored;
    REQUIRE(restored.deserialize(buffer));
    const auto& dst = restored.ecs().singleton<PresentationEvents>();
    REQUIRE(dst.events.size() == src.events.size());
    CHECK(dst.nextSeq == src.nextSeq);
    for (size_t i = 0; i < src.events.size(); ++i)
    {
        CHECK(dst.events[i].frame == src.events[i].frame);
        CHECK(dst.events[i].entity.value == src.events[i].entity.value);
        CHECK(dst.events[i].ordinal == src.events[i].ordinal);
        CHECK(dst.events[i].seq == src.events[i].seq);
        CHECK(dst.events[i].payload.index() == src.events[i].payload.index());
    }
    CHECK(hasPayload<WhiteFrame>(dst));
}
