#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/GameDef.h"
#include "mugen/avatar/AvatarPaths.h"

namespace mugen::sim
{
namespace
{
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
    const auto& position  = world.ecs().get<TransformComponent>(entity).m_position;
    const Fixed radius    = hitboxes.m_radius;
    hitboxes.m_damage     = {WorldBox{{position.x - radius, {}, position.z - radius}, {radius * Fixed::fromInt(2), {},
                             radius * Fixed::fromInt(2)}}};
}
// 房间实体的公共基础装配。
Entity base(World& world, SpawnKind kind, int32_t configId, const TransformComponent& transform, int32_t radius,
            int32_t resSpineId, bool withMotion)
{
    const Entity entity = world.ecs().create();
    auto& ecs           = world.ecs();
    IdentityComponent identity;
    identity.m_kind       = kind;
    identity.m_configId   = configId;
    identity.m_camp       = 1;
    identity.m_hostileMask = 30;
    identity.m_roleType   = 0;
    identity.m_level      = 1;
    identity.m_born       = {world.frame(), SpawnStage::Room};
    ecs.add(entity, identity);

    TransformComponent t = transform;
    t.m_scale            = spineScale(resSpineId);
    ecs.add(entity, t);

    MotionStateComponent motion;
    if (withMotion)
    {
        motion.m_motionFile = motionFile(resSpineId);
        motion.m_motionName = "stand";
        motion.m_loop       = true;
        motion.m_playing    = true;
    }
    ecs.add(entity, motion);

    HitboxesComponent hitboxes;
    hitboxes.m_radius = Fixed::fromInt(radius);
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
    if (room.m_enabled)
        return false;
    room.m_enabled    = true;
    room.m_roomId     = config.id;
    room.m_config     = config;
    room.m_stageId    = stageId;
    room.m_copyId     = copyId;
    room.m_levelBonus = 0;
    room.m_roomType       = config.roomType;
    room.m_battleRuleType = config.battleRuleType;
    room.m_ko             = false;
    room.m_pendingKo      = false;
    room.m_haveBoss       = false;
    room.m_victory        = false;
    room.m_failure        = false;
    room.m_returnToTown   = false;
    room.m_reviving       = false;
    room.m_wave           = 0;
    room.m_waveReady      = false;
    room.m_waveTimerMs    = {};
    room.m_gameSeconds    = {};
    room.m_limitSeconds   = {};
    room.m_maxRevives     = 0;
    room.m_usedRevives    = 0;
    room.m_pvp            = world.setup().m_pvp;
    room.m_pvpTimeLimit   = world.setup().m_pvpTimeLimit;
    room.m_winnerCamp     = 0;
    room.m_transferRoom   = 0;
    room.m_transferToward = 1;
    if (const auto* copy = conf::Config::instance().find<conf::CopyConfig>(copyId))
    {
        room.m_maxRevives = copy->maxRevive;
        room.m_levelBonus = copy->addLevel;
    }
    if (room.m_pvp)
        room.m_maxRevives = 0;
    if (const auto* stage = conf::Config::instance().find<conf::StageConfig>(stageId))
        if (config.battleRuleType == 1)
            room.m_limitSeconds = Fixed::fromInt(stage->openTime);
    if (config.battleRuleType == 3)
        room.m_limitSeconds = Fixed::fromInt(at(config.battleRuleParam, 0, 0));

    // 定位已出生的玩家（非 PVP 按 actorPosX/Z，朝向 actorVectorX），并应用出生 Buff。
    size_t index = 0;
    for (const auto& [slot, entity] : world.ecs().singleton<PlayerSlots>().m_entities)
    {
        auto& transform     = world.ecs().get<TransformComponent>(entity);
        transform.m_position = {Fixed::fromInt(at(config.actorPosX, index, 0)), {},
                                Fixed::fromInt(at(config.actorPosZ, index, 0))};
        transform.m_towardX  = at(config.actorVectorX, index, 1) < 0 ? -1 : 1;
        transform.m_vectorX  = Fixed::fromInt(transform.m_towardX);
        for (const int32_t id : config.actorBuffIds)
            if (id != -1)
                buff::rules::admit(world, entity, id);
        ++index;
    }

    // 已清理房间不重复生成敌人（移植旧实现 enter 的清房判断）。
    const bool cleared =
        std::find(room.m_clearedRooms.begin(), room.m_clearedRooms.end(), config.id) != room.m_clearedRooms.end();
    if (!cleared)
    {
        for (size_t i = 0; i < config.monsterIds.size(); ++i)
        {
            const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(config.monsterIds[i]);
            if (!row || config.monsterIds[i] <= 0)
                continue;
            RoleSpawnParams params;
            params.m_roleId       = row->id;
            params.m_isAi         = true;
            params.m_aiDifficulty = config.monsterAiDifficulty;
            const int32_t type    = row->roleType;
            const size_t levelIndex = type == 6 || type == 10 || type == 12 ? 0 : static_cast<size_t>(type - 2);
            params.m_level        = at(config.monsterLevels, levelIndex, 0) + room.m_levelBonus;
            params.m_position     = {Fixed::fromInt(at(config.monsterPosX, i, 0)), {},
                                     Fixed::fromInt(at(config.monsterPosZ, i, 0))};
            params.m_toward       = at(config.monsterVectorX, i, 1);
            role::rules::spawnRole(world, params);
            room.m_haveBoss |= type == 4;
        }
    }

    load(world, config);
    // 立即实体化怪物，随后按角色类型挂上死亡掉落表。
    life::rules::spawnFlush(world, SpawnStage::Room);
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.m_kind != SpawnKind::Role || identity.m_playerSlot)
            return;
        const size_t dropIndex = identity.m_roleType >= 2 ? static_cast<size_t>(identity.m_roleType - 2) : 0;
        DropTableComponent drops;
        if (!config.monsterDropGoods.empty())
            drops.m_hpRate = Fixed::fromInt(at(config.monsterDropGoods[0], dropIndex, 0));
        if (config.monsterDropGoods.size() > 1)
            drops.m_epRate = Fixed::fromInt(at(config.monsterDropGoods[1], dropIndex, 0));
        world.ecs().add(entity, drops);
    });
    // 初始化状态机：下一帧开始按 30ms 间隔依次推进 Init → Ready → Enter → ReadySpine → Tip → Process。
    room.m_state = mugen::RoomState::Check;
    room.m_pending.assign({mugen::RoomState::Init});
    room.m_recorded.assign({mugen::RoomState::Init});
    room.m_waitMs = Fixed::fromInt(30);
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
            transform.m_position = {Fixed::fromInt(at(room.obstaclePos[i], 0)), {},
                                    Fixed::fromInt(at(room.obstaclePos[i], 1))};
        transform.m_towardX = at(room.obstacleVectorX, i, 1);
        transform.m_vectorX = Fixed::fromInt(transform.m_towardX);
        spawnObstacle(world, row->id, transform,
                      at(room.obstacleLevel, static_cast<size_t>(std::max(1, row->obstacleType) - 1), 1));
    }
    for (size_t i = 0; i < room.goodIds.size(); ++i)
    {
        if (room.goodIds[i] <= 0)
            continue;
        TransformComponent transform;
        if (i < room.goodPos.size())
            transform.m_position = {Fixed::fromInt(at(room.goodPos[i], 0)), {}, Fixed::fromInt(at(room.goodPos[i], 1))};
        transform.m_towardX = at(room.goodVectorX, i, 1);
        transform.m_vectorX = Fixed::fromInt(transform.m_towardX);
        spawnGoods(world, room.goodIds[i], transform, false);
    }
    for (size_t i = 0; i < room.portalIds.size(); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::PortalConfig>(room.portalIds[i]);
        if (!row || row->id <= 0 || at(room.portalDestType, i) != 2)
            continue;
        TransformComponent transform;
        transform.m_position = {Fixed::fromInt(at(room.portalPosX, i)), {}, Fixed::fromInt(at(room.portalPosZ, i))};
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
    state.m_configId = id;
    state.m_hits     = row->hitCounts;
    state.m_level    = level;
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
    placed.m_position.y       = {};
    placed.m_position.x += Fixed::fromInt(at(row->relativePosition, 0)) * Fixed::fromInt(transform.m_towardX);
    placed.m_position.z += Fixed::fromInt(at(row->relativePosition, 2));
    const Entity entity = base(world, SpawnKind::Goods, id, placed, row->radius, row->resSpineId, true);
    GoodsStateComponent state;
    state.m_configId = id;
    state.m_phase    = spurt ? GoodsPhase::Spurt : GoodsPhase::Idle;
    if (spurt)
    {
        auto& random     = world.ecs().singleton<RngState>().m_random;
        state.m_velocity = {random.randomF("-0.3"_fx, "0.3"_fx), "0.8"_fx, random.randomF("-0.16"_fx, "0.16"_fx)};
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
    state.m_configId    = id;
    state.m_roomId      = destRoom;
    state.m_toward      = toward < 0 ? -1 : 1;
    state.m_limit       = limit;
    state.m_destination = destination;
    world.ecs().add(entity, state);
    writeRadiusDamage(world, entity);
    return entity;
}

// 追加一条掉落物生成请求（由下一同步点实体化）。
void pushGoods(World& world, int32_t id, const TransformComponent& transform)
{
    SpawnRequest request;
    request.m_kind     = SpawnKind::Goods;
    request.m_configId = id;
    request.m_position = transform.m_position;
    request.m_towardX  = transform.m_towardX;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}

// 结算实体的死亡掉落。
void drop(World& world, Entity entity)
{
    auto& ecs           = world.ecs();
    const auto& transform = ecs.get<TransformComponent>(entity);
    const auto* identity = ecs.tryGet<IdentityComponent>(entity);
    const int32_t roleType = identity ? identity->m_roleType : 0;
    if (roleType == 1)
        return;
    const auto* table = ecs.tryGet<DropTableComponent>(entity);
    const DropTableComponent data = table ? *table : DropTableComponent{};
    for (const int32_t id : data.m_goods)
        pushGoods(world, id, transform);
    if (ecs.has<ObstacleStateComponent>(entity))
        return;
    // 掉落概率与抛射速度使用原始随机流（对齐旧实现 RoomDirector::drop，不参与 DrawPolicy）。
    auto& random = ecs.singleton<RngState>().m_random;
    // 属性掉落（扩展属性 108，移植旧实现 drop 的 bonus 药品）。
    if (const auto* stats = ecs.tryGet<StatsComponent>(entity))
        if (stats->m_extend[8] > Fixed{} && random.randomF(Fixed{}, Fixed::fromInt(100)) < stats->m_extend[8])
            pushGoods(world, 6, transform);
    if (roleType >= 2 && roleType <= 4)
    {
        if (data.m_hpRate > Fixed{} && random.randomF(Fixed{}, Fixed::fromInt(100)) < data.m_hpRate)
            pushGoods(world, 6, transform);
        if (data.m_epRate > Fixed{} && random.randomF(Fixed{}, Fixed::fromInt(100)) < data.m_epRate)
            pushGoods(world, 7, transform);
    }
}

// 胜利后开放全部关闭状态的传送门。
void openPortals(World& world)
{
    world.ecs().each<PortalStateComponent>([&](Entity, PortalStateComponent& state) {
        if (state.m_phase == PortalPhase::Close)
            state.m_phase = PortalPhase::Active;
    });
}

// 按生成请求装配房间实体（供 SpawnFlushSystem 在同步点调用）。
Entity materialize(World& world, const SpawnRequest& request, SpawnStage stage)
{
    TransformComponent transform;
    transform.m_position = request.m_position;
    transform.m_towardX  = request.m_towardX == -1 ? -1 : 1;
    transform.m_vectorX  = Fixed::fromInt(transform.m_towardX);
    if (request.m_kind == SpawnKind::Obstacle)
        return spawnObstacle(world, request.m_configId, transform, request.m_level);
    if (request.m_kind == SpawnKind::Goods)
        return spawnGoods(world, request.m_configId, transform, true);
    static_cast<void>(stage);
    return {};
}
}  // namespace room::rules
}  // namespace mugen::sim
