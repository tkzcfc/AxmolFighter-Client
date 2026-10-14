#include "RoomEntities.h"
#include "mugen/sim/World.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/ability/AbilityHolder.h"
#include "mugen/sim/obstacle/Obstacle.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/avatar/MotionPlayer.h"
#include "mugen/conf/BattleConst.h"

namespace mugen::sim::room_entities
{
namespace
{
// 配置数组允许尾部省略，负一仍保持为空标记。
int32_t at(const std::vector<int32_t>& list, size_t i, int32_t fallback = 0)
{
    return i < list.size() ? list[i] : fallback;
}
// 目标列表按配置语义只读取首个目标。
int32_t first(const std::vector<std::vector<int32_t>>& list, size_t i, int32_t fallback = 0)
{
    return i < list.size() ? at(list[i], 0, fallback) : fallback;
}
// 没有战斗动作时间轴时，按当前位置和配置半径写受击盒。
void writeRadiusDamage(World& world, Entity entity)
{
    auto& ecs           = world.ecs();
    auto& collider      = ecs.get<ColliderState>(entity);
    const auto position = ecs.get<Transform>(entity).position;
    const auto radius   = collider.radius;
    collider.damage     = {
        {{position.x - radius, {}, position.y + position.z - radius}, {radius * "2"_fx, {}, radius * "2"_fx}}};
}
// 切换动作时清除循环计数，供完成门控读取。
void play(World& world, Entity entity, const std::string& name, bool loop)
{
    auto& motion            = world.ecs().get<MotionState>(entity);
    motion.motionName       = name;
    motion.timeMs           = {};
    motion.m_completedLoops = 0;
    motion.loop             = loop;
    motion.playing          = true;
}
// 非角色动画独立采样，不调用角色攻击树完成回调。
bool animate(World& world, Entity entity)
{
    auto& ecs    = world.ecs();
    auto& motion = ecs.get<MotionState>(entity);
    MotionPlayer player;
    if (motion.motionFile.empty() || !player.bind(motion.motionFile) ||
        !player.play(motion.motionName, {}, motion.loop))
    {
        writeRadiusDamage(world, entity);
        return true;
    }
    const auto duration = Fixed::fromInt(player.getDurationMs());
    motion.timeMs += ecs.singleton<WorldClock>().dtMs * motion.actionScale;
    const bool complete = duration <= Fixed{} || motion.timeMs >= duration;
    if (complete)
    {
        ++motion.m_completedLoops;
        if (motion.loop && duration > Fixed{})
            motion.timeMs = Fixed::fromRaw(motion.timeMs.raw % duration.raw);
        else
            motion.timeMs = duration;
    }
    player.seek(static_cast<int32_t>(motion.timeMs.toInt()));
    std::vector<const DamageBox*> attack, damage;
    player.boxesAt(attack, damage);
    auto& collider = ecs.get<ColliderState>(entity);
    collider.attack.clear();
    collider.damage.clear();
    const auto transform = ecs.get<Transform>(entity);
    for (const auto* box : attack)
        collider.attack.push_back(role::transformBox(*box, transform));
    for (const auto* box : damage)
        collider.damage.push_back(role::transformBox(*box, transform));
    return complete;
}
// 生成房间实体所需的基础组件，碰撞形状由逻辑动画采样。
Entity base(World& world, GroupKind group, Transform transform, int32_t spineId, int32_t radius)
{
    const auto entity = world.create(group);
    auto& ecs         = world.ecs();
    MotionState motion;
    if (const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(spineId))
    {
        // 传送门只用 Spine 外观和半径碰撞，不绑定战斗动作时间轴。
        if (group != GroupKind::Portal)
            motion.motionFile = AvatarPaths::motionFileFromSpine(spine->fileName);
        transform.scale = spine->scale;
    }
    RoleIdentity identity;
    identity.roleType    = 0;
    identity.camp        = 1;
    identity.hostileCamp = 30;
    ecs.add(entity, identity);
    ecs.add(entity, transform);
    ecs.add(entity, motion);
    ecs.add(entity, RoleStatus{});
    ecs.add(entity, RoleHitState{});
    ecs.add(entity, ExtraState{});
    ecs.add(entity, Displacement{});
    ecs.add(entity, ColliderState{{}, {}, ecs.singleton<WorldClock>().frame, Fixed::fromInt(radius)});
    ecs.add(entity, CollisionState{});
    play(world, entity, "stand", true);
    return entity;
}
// 门动画按阶段索引读取；没有配置时使用明确的动作名。
std::string portalMotion(const conf::PortalConfig& row, PortalPhase phase)
{
    const size_t index = phase == PortalPhase::Transfer ? 0 : phase == PortalPhase::Active ? 1 : 4;
    if (index < row.spineAnimations.size() && !row.spineAnimations[index].empty())
    {
        const auto& value = row.spineAnimations[index][0];
        if (const auto* name = std::get_if<std::string>(&value))
            return *name;
        return std::to_string(std::get<int32_t>(value));
    }
    return phase == PortalPhase::Close ? "close" : phase == PortalPhase::Active ? "active" : "transfer";
}
}  // namespace

Entity goods(World& world, int32_t id, Transform transform, bool spurt)
{
    const auto* row = conf::Config::instance().find<conf::GoodsConfig>(id);
    if (!row || id <= 0)
        return {};
    transform.position.y = {};
    transform.position.x += Fixed::fromInt(at(row->relativePosition, 0) * transform.towardX);
    transform.position.z += Fixed::fromInt(at(row->relativePosition, 2));
    const auto entity = base(world, GroupKind::Goods, transform, row->resSpineId, row->radius);
    GoodsState state;
    state.m_id    = id;
    state.m_phase = spurt ? GoodsPhase::Spurt : GoodsPhase::Idle;
    if (spurt)
    {
        auto& random     = world.ecs().singleton<RngState>().random;
        state.m_velocity = {random.randomF("-0.3"_fx, "0.3"_fx), "0.8"_fx, random.randomF("-0.16"_fx, "0.16"_fx)};
        play(world, entity, "spurt", true);
    }
    world.ecs().add(entity, state);
    return entity;
}

Entity obstacle(World& world, int32_t id, Transform transform, int32_t level)
{
    const auto* row = conf::Config::instance().find<conf::ObstacleConfig>(id);
    if (!row || id <= 0)
        return {};
    const auto entity = base(world, GroupKind::Obstacle, transform, row->resSpineId, row->radius);
    world.ecs().add(entity, ObstacleState{id, row->hitCounts, {}});
    conf::RoleConfig attributes;
    attributes.roleType = 2;
    for (const auto value : row->attributeRate)
        attributes.attributeRate.push_back(Fixed::fromInt(value));
    world.ecs().add(entity, loadAttributes(attributes, level));
    world.ecs().add(entity, DropGoods{});
    return entity;
}

void load(World& world, const conf::RoomConfig& room)
{
    for (size_t i = 0; i < room.obstacleIds.size(); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::ObstacleConfig>(room.obstacleIds[i]);
        if (!row)
            continue;
        Transform transform;
        if (i < room.obstaclePos.size())
            transform.position = {
                Fixed::fromInt(at(room.obstaclePos[i], 0)), {}, Fixed::fromInt(at(room.obstaclePos[i], 1))};
        transform.towardX = at(room.obstacleVectorX, i, 1);
        transform.vectorX = Fixed::fromInt(transform.towardX);
        obstacle(world, row->id, transform,
                 at(room.obstacleLevel, static_cast<size_t>(std::max(1, row->obstacleType) - 1), 1));
    }
    for (size_t i = 0; i < room.goodIds.size(); ++i)
    {
        Transform transform;
        if (i < room.goodPos.size())
            transform.position = {Fixed::fromInt(at(room.goodPos[i], 0)), {}, Fixed::fromInt(at(room.goodPos[i], 1))};
        transform.towardX = at(room.goodVectorX, i, 1);
        transform.vectorX = Fixed::fromInt(transform.towardX);
        goods(world, room.goodIds[i], transform, false);
    }
    for (size_t i = 0; i < room.portalIds.size(); ++i)
    {
        const auto* row = conf::Config::instance().find<conf::PortalConfig>(room.portalIds[i]);
        if (!row || row->id <= 0 || at(room.portalDestType, i) != 2)
            continue;
        Transform transform;
        transform.position = {Fixed::fromInt(at(room.portalPosX, i)), {}, Fixed::fromInt(at(room.portalPosZ, i))};
        const auto entity  = base(world, GroupKind::Portal, transform, row->resSpineId, row->radius);
        PortalState state;
        state.m_id          = row->id;
        state.m_room        = first(room.portalDestRoomIds, i);
        state.m_toward      = first(room.portalDestVectorX, i, 1);
        state.m_limit       = at(room.portalLimitType, i);
        state.m_destination = {
            Fixed::fromInt(first(room.portalDestPosX, i)), {}, Fixed::fromInt(first(room.portalDestPosZ, i))};
        world.ecs().add(entity, state);
        play(world, entity, portalMotion(*row, state.m_phase), true);
    }
}

void openPortals(World& world)
{
    world.ecs().each<PortalState>([&](Entity entity, PortalState& state) {
        if (state.m_phase != PortalPhase::Close)
            return;
        state.m_phase   = PortalPhase::Active;
        const auto* row = conf::Config::instance().find<conf::PortalConfig>(state.m_id);
        if (row)
            play(world, entity, portalMotion(*row, state.m_phase), true);
    });
}

void update(World& world, Entity entity)
{
    auto& ecs     = world.ecs();
    const auto dt = ecs.singleton<WorldClock>().dtMs;
    if (auto* state = ecs.tryGet<GoodsState>(entity))
    {
        if (state->m_phase == GoodsPhase::Spurt)
        {
            auto& position      = ecs.get<Transform>(entity).position;
            const auto previous = position;
            const auto step     = displacement::integrate({}, state->m_velocity.y, {}, {}, {}, dt, conf::kGravity);
            position.x += state->m_velocity.x * dt;
            position.z += state->m_velocity.z * dt * conf::kZRate;
            position.y += step.offset;
            state->m_velocity.y = step.velocity;
            if (position.y <= Fixed{} && step.velocity < Fixed{})
            {
                position.y = {};
                if (step.velocity < -conf::kBounces)
                    state->m_velocity.y = -step.velocity * "0.2"_fx;
                else
                {
                    state->m_phase    = GoodsPhase::Idle;
                    state->m_velocity = {};
                    play(world, entity, "stand", true);
                }
            }
            ecs.get<Displacement>(entity).offset = position - previous;
        }
        const auto phase = state->m_phase;
        if (animate(world, entity) && phase == GoodsPhase::Collect)
            world.destroy(entity);
    }
    else if (auto* state = ecs.tryGet<ObstacleState>(entity))
    {
        const auto* row = conf::Config::instance().find<conf::ObstacleConfig>(state->m_id);
        if (state->m_hits == 0 && !state->m_deathStarted)
        {
            state->m_deathStarted    = true;
            state->m_displacementEnd = !row || row->deathDisplacementId == -1;
            if (row)
                displacement::set(world, entity, row->deathDisplacementId);
        }
        if (state->m_deathStarted && !state->m_displacementEnd)
            displacement::update(world, entity);
        state->m_time += dt;
        const bool complete = animate(world, entity);
        if (state->m_hits == 0 && state->m_displacementEnd && complete && state->m_time > Fixed{})
        {
            drop(world, entity);
            if (row && row->deathEffectId != -1)
            {
                effect::RoleSpawn request;
                request.m_effectId = row->deathEffectId;
                effect::spawnFromRole(world, entity, request);
            }
            world.destroy(entity);
        }
        else if (complete && ecs.get<MotionState>(entity).motionName == "hit")
            play(world, entity, "stand", true);
    }
    else if (const auto* portal = ecs.tryGet<PortalState>(entity))
    {
        const auto state = *portal;
        writeRadiusDamage(world, entity);
        if (state.m_phase == PortalPhase::Transfer)
        {
            auto& room = ecs.singleton<RoomState>();
            if (room.m_transferRoom == 0)
            {
                room.m_transferRoom     = state.m_room;
                room.m_transferPosition = state.m_destination;
                room.m_transferToward   = state.m_toward;
            }
        }
    }
}

void collision(World& world, Entity entity, Entity other, CollisionEvent event)
{
    if (event == CollisionEvent::End)
        return;
    auto& ecs = world.ecs();
    if (ecs.get<EntityGroup>(entity).destroyRequested || ecs.get<EntityGroup>(other).suspended)
        return;
    if (ecs.has<GoodsState>(entity) && ecs.has<ObstacleState>(other))
    {
        obstacle::onCollision(world, entity, other, event, {});
        return;
    }
    const auto* identity = ecs.tryGet<RoleIdentity>(other);
    if (!identity || ecs.get<EntityGroup>(other).kind != GroupKind::Role)
        return;
    const auto& players = ecs.singleton<PlayerSlots>().entities;
    const bool controlled =
        std::any_of(players.begin(), players.end(), [other](const auto& slot) { return slot.second == other; });
    if (identity->roleType != 1 && !controlled)
        return;
    if (const auto* status = ecs.tryGet<RoleStatus>(other); status && status->isDeath())
        return;
    if (auto* portal = ecs.tryGet<PortalState>(entity))
    {
        const auto* row = conf::Config::instance().find<conf::PortalConfig>(portal->m_id);
        if (!row)
            return;
        if (portal->m_phase == PortalPhase::Active && ecs.singleton<RoomState>().m_victory)
        {
            portal->m_phase = PortalPhase::Transfer;
            play(world, entity, portalMotion(*row, portal->m_phase), false);
        }
        else if (portal->m_phase == PortalPhase::Close && event == CollisionEvent::Began && !row->closeSystemPx.empty())
            Role(world, other).changeStatus(EntityRoleStatus::Jostled, row->closeSystemPx[0]);
    }
    else if (auto* state = ecs.tryGet<GoodsState>(entity))
    {
        if (state->m_phase != GoodsPhase::Idle)
            return;
        const auto* row = conf::Config::instance().find<conf::GoodsConfig>(state->m_id);
        if (!row)
            return;
        state->m_phase       = GoodsPhase::Collect;
        state->m_collector   = other;
        const auto transform = ecs.get<Transform>(entity);
        const auto own       = *identity;
        play(world, entity, "death", false);
        if (row->buffId != -1)
            buff::addEntityBuff(world, other, row->buffId);
        if (row->specialabilityId != -1)
            ability::AbilityHolder(world, other).add(row->specialabilityId);
        if (row->soundPick != -1)
            world.emit(entity, Sound{row->soundPick, {}, false, 0});
        if (row->summonId > 0)
        {
            RoleSpawnParams params;
            params.roleId   = row->summonId;
            params.camp     = own.camp;
            params.level    = own.level;
            params.m_owner  = other;
            params.isAi     = true;
            params.position = transform.position;
            params.toward   = ecs.get<Transform>(other).towardX;
            spawnRole(world, params);
        }
    }
}

void drop(World& world, Entity entity)
{
    auto& ecs            = world.ecs();
    const auto transform = ecs.get<Transform>(entity);
    const auto* identity = ecs.tryGet<RoleIdentity>(entity);
    const auto roleType  = identity ? identity->roleType : 0;
    if (roleType == 1)
        return;
    const auto* drops = ecs.tryGet<DropGoods>(entity);
    const auto data   = drops ? *drops : DropGoods{};
    for (const auto id : data.m_goods)
        goods(world, id, transform, true);
    if (ecs.has<ObstacleState>(entity))
        return;
    auto& random = ecs.singleton<RngState>().random;
    if (const auto* attributes = ecs.tryGet<Attributes>(entity))
        if (attributes->extend[8] > Fixed{} && random.randomF({}, "100"_fx) < attributes->extend[8])
            goods(world, 6, transform, true);
    if (roleType >= 2 && roleType <= 4)
    {
        if (data.m_hpRate > Fixed{} && random.randomF({}, "100"_fx) < data.m_hpRate)
            goods(world, 6, transform, true);
        if (data.m_epRate > Fixed{} && random.randomF({}, "100"_fx) < data.m_epRate)
            goods(world, 7, transform, true);
    }
}
bool displacementEvent(World& world, Entity entity, DisplacementEvent event)
{
    auto& state = world.ecs().get<ObstacleState>(entity);
    if (event == DisplacementEvent::Brake || event == DisplacementEvent::Lie)
        state.m_displacementEnd = true;
    return false;
}
}  // namespace mugen::sim::room_entities
