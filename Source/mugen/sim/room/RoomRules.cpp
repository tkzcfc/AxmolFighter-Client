#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include "mugen/avatar/AvatarPaths.h"
#include <array>

namespace mugen::sim
{
namespace
{
// PVP 决斗的右侧队伍阵营位；其余阵营归入左侧队伍。
constexpr int32_t kPvpRightCamp = 2;
// PVP 决斗左右两队的出生横坐标；左队朝右，右队朝左。
constexpr int32_t kPvpLeftX  = 300;
constexpr int32_t kPvpRightX = 1900;
// PVP 同队成员按出生顺序占用的纵深坐标。
constexpr std::array<int32_t, 3> kPvpSpawnZ = {200, 150, 50};
// 读取配置数组中可省略的尾项。
int32_t at(const std::vector<int32_t>& values, size_t index, int32_t fallback = 0)
{
    return index < values.size() ? values[index] : fallback;
}
// 目标列表按配置语义只读取首个目标。
int32_t first(const std::vector<std::vector<int32_t>>& list, size_t i, int32_t fallback = 0)
{
    return i < list.size() ? at(list[i], 0, fallback) : fallback;
}
// 由 resSpineId 解析动作文件。
std::string motionFile(int32_t resSpineId)
{
    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    return spine ? AvatarPaths::motionFileFromSpine(spine->fileName) : std::string{};
}
// 骨骼缩放。
Fixed spineScale(int32_t resSpineId)
{
    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    return spine ? spine->scale : Fixed::fromInt(1);
}
// 按当前位置与配置半径写受击盒（房间实体没有战斗动作时间轴）。
void writeRadiusDamage(World& world, Entity entity)
{
    auto& hitboxes        = world.ecs().get<HitboxesComponent>(entity);
    const auto& position  = world.ecs().get<TransformComponent>(entity).position;
    const Fixed radius = hitboxes.radius;
    const Fixed span   = radius * Fixed::fromInt(2);
    // 没有动作框时用半径铺横向和纵深，高度同样取直径，避免某一轴尺寸为 0。
    hitboxes.damage  = {WorldBox{{position.x - radius, position.z - radius, position.y}, {span, span, span}}};
}
// 房间实体的公共基础装配。
Entity base(World& world, SpawnKind kind, int32_t configId, const TransformComponent& transform, int32_t radius,
            int32_t resSpineId, bool withMotion)
{
    const Entity entity = world.ecs().create();
    auto& ecs           = world.ecs();
    IdentityComponent identity;
    identity.kind       = kind;
    identity.configId   = configId;
    identity.camp       = 1;
    identity.hostileMask = 30;
    identity.roleType   = 0;
    identity.level      = 1;
    identity.born       = {world.frame(), SpawnStage::Room};
    ecs.add(entity, identity);

    TransformComponent t = transform;
    t.scale            = spineScale(resSpineId);
    ecs.add(entity, t);

    MotionStateComponent motion;
    if (withMotion)
    {
        motion.motionFile = motionFile(resSpineId);
        motion.motionName = "stand";
        motion.loop       = true;
        motion.playing    = true;
    }
    ecs.add(entity, motion);

    HitboxesComponent hitboxes;
    hitboxes.radius = Fixed::fromInt(radius);
    ecs.add(entity, hitboxes);
    ecs.add(entity, ContactMemoryComponent{});
    return entity;
}
}  // namespace

namespace room::rules
{
// 进入战斗房间：定位玩家、生成怪物与房间实体并初始化房间状态机。
bool enter(World& world, const conf::RoomConfig& config, int32_t stageId, int32_t copyId)
{
    auto& room = world.ecs().singleton<RoomState>();
    if (room.enabled)
        return false;
    room.enabled    = true;
    room.roomId     = config.id;
    room.config     = config;
    room.stageId    = stageId;
    room.copyId     = copyId;
    room.levelBonus = 0;
    room.roomType       = config.roomType;
    room.battleRuleType = config.battleRuleType;
    room.ko             = false;
    room.pendingKo      = false;
    room.haveBoss       = false;
    room.victory        = false;
    room.failure        = false;
    room.returnToTown   = false;
    room.reviving       = false;
    room.wave           = 0;
    room.waveReady      = false;
    room.waveTimerMs    = {};
    room.gameSeconds    = {};
    room.limitSeconds   = {};
    room.maxRevives     = 0;
    room.usedRevives    = 0;
    room.pvp            = world.setup().pvp;
    room.pvpTimeLimit   = world.setup().pvpTimeLimit;
    room.winnerCamp     = 0;
    room.transferRoom   = 0;
    room.transferToward = 1;
    if (const auto* copy = conf::Config::instance().find<conf::CopyConfig>(copyId))
    {
        room.maxRevives = copy->maxRevive;
        room.levelBonus = copy->addLevel;
    }
    if (room.pvp)
        room.maxRevives = 0;
    if (const auto* stage = conf::Config::instance().find<conf::StageConfig>(stageId))
        if (config.battleRuleType == 1)
            room.limitSeconds = Fixed::fromInt(stage->openTime);
    if (config.battleRuleType == 3)
        room.limitSeconds = Fixed::fromInt(at(config.battleRuleParam, 0, 0));

    // 定位已出生的玩家（PVP 按阵营分左右两队，非 PVP 按 actorPosX/Z，朝向 actorVectorX），并应用出生 Buff。
    size_t index = 0;
    std::array<size_t, 2> teamCounts{};
    for (const auto& [slot, entity] : world.ecs().singleton<PlayerSlots>().entities)
    {
        auto& transform = world.ecs().get<TransformComponent>(entity);
        if (room.pvp)
        {
            const bool rightTeam = world.ecs().get<IdentityComponent>(entity).camp == kPvpRightCamp;
            const size_t team    = rightTeam ? 1 : 0;
            const size_t lane    = std::min(teamCounts[team]++, kPvpSpawnZ.size() - 1);
            transform.position   = {Fixed::fromInt(rightTeam ? kPvpRightX : kPvpLeftX), {},
                                    Fixed::fromInt(kPvpSpawnZ[lane])};
            transform.towardX    = rightTeam ? -1 : 1;
            transform.vectorX    = Fixed::fromInt(transform.towardX);
        }
        else
        {
            transform.position = {Fixed::fromInt(at(config.actorPosX, index, 0)), {},
                                  Fixed::fromInt(at(config.actorPosZ, index, 0))};
            transform.towardX  = at(config.actorVectorX, index, 1) < 0 ? -1 : 1;
            transform.vectorX  = Fixed::fromInt(transform.towardX);
        }
        for (const int32_t id : config.actorBuffIds)
            if (id != -1)
                buff::rules::admit(world, entity, id);
        ++index;
    }

    // 已清理房间不重复生成敌人（移植旧实现 enter 的清房判断）。
    const bool cleared =
        std::find(room.clearedRooms.begin(), room.clearedRooms.end(), config.id) != room.clearedRooms.end();
    if (!cleared)
    {
        for (size_t i = 0; i < config.monsterIds.size(); ++i)
        {
            const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(config.monsterIds[i]);
            if (!row || config.monsterIds[i] <= 0)
                continue;
            RoleSpawnParams params;
            params.roleId       = row->id;
            params.isAi         = true;
            params.aiDifficulty = config.monsterAiDifficulty;
            const int32_t type    = row->roleType;
            const size_t levelIndex = type == 6 || type == 10 || type == 12 ? 0 : static_cast<size_t>(type - 2);
            params.level        = at(config.monsterLevels, levelIndex, 0) + room.levelBonus;
            params.position     = {Fixed::fromInt(at(config.monsterPosX, i, 0)), {},
                                     Fixed::fromInt(at(config.monsterPosZ, i, 0))};
            params.toward       = at(config.monsterVectorX, i, 1);
            role::rules::spawnRole(world, params);
            room.haveBoss |= type == 4;
        }
    }

    load(world, config);
    // 立即实体化怪物，随后按角色类型挂上死亡掉落表。
    life::rules::spawnFlush(world, SpawnStage::Room);
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.kind != SpawnKind::Role || identity.playerSlot)
            return;
        const size_t dropIndex = identity.roleType >= 2 ? static_cast<size_t>(identity.roleType - 2) : 0;
        DropTableComponent drops;
        if (!config.monsterDropGoods.empty())
            drops.hpRate = Fixed::fromInt(at(config.monsterDropGoods[0], dropIndex, 0));
        if (config.monsterDropGoods.size() > 1)
            drops.epRate = Fixed::fromInt(at(config.monsterDropGoods[1], dropIndex, 0));
        world.ecs().add(entity, drops);
    });
    // 初始化状态机：下一帧开始按 30ms 间隔依次推进 Init → Ready → Enter → ReadySpine → Tip → Process。
    room.state = mugen::RoomState::Check;
    room.pending.assign({mugen::RoomState::Init});
    room.recorded.assign({mugen::RoomState::Init});
    room.waitMs = Fixed::fromInt(30);
    return true;
}

// 按房间配置生成障碍、物品与传送门。
void load(World& world, const conf::RoomConfig& room)
{
    for (size_t i = 0; i < room.obstacleIds.size(); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::ObstacleConfig>(room.obstacleIds[i]);
        if (!row || row->id <= 0)
            continue;
        TransformComponent transform;
        if (i < room.obstaclePos.size())
            transform.position = {Fixed::fromInt(at(room.obstaclePos[i], 0)), {},
                                    Fixed::fromInt(at(room.obstaclePos[i], 1))};
        transform.towardX = at(room.obstacleVectorX, i, 1);
        transform.vectorX = Fixed::fromInt(transform.towardX);
        spawnObstacle(world, row->id, transform,
                      at(room.obstacleLevel, static_cast<size_t>(std::max(1, row->obstacleType) - 1), 1));
    }
    for (size_t i = 0; i < room.goodIds.size(); ++i)
    {
        if (room.goodIds[i] <= 0)
            continue;
        TransformComponent transform;
        if (i < room.goodPos.size())
            transform.position = {Fixed::fromInt(at(room.goodPos[i], 0)), {}, Fixed::fromInt(at(room.goodPos[i], 1))};
        transform.towardX = at(room.goodVectorX, i, 1);
        transform.vectorX = Fixed::fromInt(transform.towardX);
        spawnGoods(world, room.goodIds[i], transform, false);
    }
    for (size_t i = 0; i < room.portalIds.size(); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::PortalConfig>(room.portalIds[i]);
        if (!row || row->id <= 0 || at(room.portalDestType, i) != 2)
            continue;
        TransformComponent transform;
        transform.position = {Fixed::fromInt(at(room.portalPosX, i)), {}, Fixed::fromInt(at(room.portalPosZ, i))};
        spawnPortal(world, row->id, transform, first(room.portalDestRoomIds, i), first(room.portalDestVectorX, i, 1),
                    at(room.portalLimitType, i),
                    {Fixed::fromInt(first(room.portalDestPosX, i)), {}, Fixed::fromInt(first(room.portalDestPosZ, i))});
    }
}

// 生成单个障碍物。
Entity spawnObstacle(World& world, int32_t id, const TransformComponent& transform, int32_t level)
{
    const auto* row = conf::Config::instance().find<conf::ObstacleConfig>(id);
    if (!row || id <= 0)
        return {};
    const Entity entity = base(world, SpawnKind::Obstacle, id, transform, row->radius, row->resSpineId, true);
    ObstacleStateComponent state;
    state.configId = id;
    state.hits     = row->hitCounts;
    state.level    = level;
    world.ecs().add(entity, state);
    world.ecs().add(entity, DropTableComponent{});
    writeRadiusDamage(world, entity);
    return entity;
}

// 生成单个物品。
Entity spawnGoods(World& world, int32_t id, const TransformComponent& transform, bool spurt)
{
    const auto* row = conf::Config::instance().find<conf::GoodsConfig>(id);
    if (!row || id <= 0)
        return {};
    TransformComponent placed = transform;
    placed.position.y       = {};
    placed.position.x += Fixed::fromInt(at(row->relativePosition, 0)) * Fixed::fromInt(transform.towardX);
    placed.position.z += Fixed::fromInt(at(row->relativePosition, 2));
    const Entity entity = base(world, SpawnKind::Goods, id, placed, row->radius, row->resSpineId, true);
    GoodsStateComponent state;
    state.configId = id;
    state.phase    = spurt ? GoodsPhase::Spurt : GoodsPhase::Idle;
    if (spurt)
    {
        auto& random     = world.ecs().singleton<RngState>().random;
        state.velocity = {random.randomF("-0.3"_fx, "0.3"_fx), "0.8"_fx, random.randomF("-0.16"_fx, "0.16"_fx)};
    }
    world.ecs().add(entity, state);
    writeRadiusDamage(world, entity);
    return entity;
}

// 生成单个传送门。
Entity spawnPortal(World& world, int32_t id, const TransformComponent& transform, int32_t destRoom, int32_t toward,
                   int32_t limit, const FixedVec3& destination)
{
    const auto* row = conf::Config::instance().find<conf::PortalConfig>(id);
    if (!row || id <= 0)
        return {};
    const Entity entity = base(world, SpawnKind::Portal, id, transform, row->radius, row->resSpineId, false);
    PortalStateComponent state;
    state.configId    = id;
    state.roomId      = destRoom;
    state.toward      = toward < 0 ? -1 : 1;
    state.limit       = limit;
    state.destination = destination;
    world.ecs().add(entity, state);
    writeRadiusDamage(world, entity);
    return entity;
}

// 追加一条掉落物生成请求（由下一同步点实体化）。
void pushGoods(World& world, int32_t id, const TransformComponent& transform)
{
    SpawnRequest request;
    request.kind     = SpawnKind::Goods;
    request.configId = id;
    request.position = transform.position;
    request.towardX  = transform.towardX;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}

// 结算实体的死亡掉落。
void drop(World& world, Entity entity)
{
    auto& ecs           = world.ecs();
    const auto& transform = ecs.get<TransformComponent>(entity);
    const auto* identity = ecs.tryGet<IdentityComponent>(entity);
    const int32_t roleType = identity ? identity->roleType : 0;
    if (roleType == 1)
        return;
    const auto* table = ecs.tryGet<DropTableComponent>(entity);
    const DropTableComponent data = table ? *table : DropTableComponent{};
    for (const int32_t id : data.goods)
        pushGoods(world, id, transform);
    if (ecs.has<ObstacleStateComponent>(entity))
        return;
    // 掉落概率与抛射速度使用原始随机流（对齐旧实现 RoomDirector::drop，不参与 DrawPolicy）。
    auto& random = ecs.singleton<RngState>().random;
    // 属性掉落（扩展属性 108，移植旧实现 drop 的 bonus 药品）。
    if (const auto* stats = ecs.tryGet<StatsComponent>(entity))
        if (stats->extend[8] > Fixed{} && random.randomF(Fixed{}, Fixed::fromInt(100)) < stats->extend[8])
            pushGoods(world, 6, transform);
    if (roleType >= 2 && roleType <= 4)
    {
        if (data.hpRate > Fixed{} && random.randomF(Fixed{}, Fixed::fromInt(100)) < data.hpRate)
            pushGoods(world, 6, transform);
        if (data.epRate > Fixed{} && random.randomF(Fixed{}, Fixed::fromInt(100)) < data.epRate)
            pushGoods(world, 7, transform);
    }
}

// 胜利后开放全部关闭状态的传送门。
void openPortals(World& world)
{
    world.ecs().each<PortalStateComponent>([&](Entity, PortalStateComponent& state) {
        if (state.phase == PortalPhase::Close)
            state.phase = PortalPhase::Active;
    });
}

// 按生成请求装配房间实体（供 SpawnFlushSystem 在同步点调用）。
Entity materialize(World& world, const SpawnRequest& request, SpawnStage stage)
{
    TransformComponent transform;
    transform.position = request.position;
    transform.towardX  = request.towardX == -1 ? -1 : 1;
    transform.vectorX  = Fixed::fromInt(transform.towardX);
    if (request.kind == SpawnKind::Obstacle)
        return spawnObstacle(world, request.configId, transform, request.level);
    if (request.kind == SpawnKind::Goods)
        return spawnGoods(world, request.configId, transform, true);
    static_cast<void>(stage);
    return {};
}
}  // namespace room::rules
}  // namespace mugen::sim
