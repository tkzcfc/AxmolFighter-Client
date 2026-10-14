#include "World.h"
#include "buff/BuffPool.h"
#include "room/RoomFlow.h"
#include "role/brain/RoleBrainRunner.h"
#include "trace/TraceSink.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
// 作为未配置实体组阶段时的空更新回调。
void WorldHooks::noEntityUpdate(World&, Entity) {}
// 作为未配置世界阶段时的空更新回调。
void WorldHooks::noWorldUpdate(World&) {}

// 重置本场 ECS，并从有效房间设置初始化确定性单例状态。
void World::init(WorldSetup setup)
{
    const bool validMode = setup.mode == WorldMode::Battle || setup.mode == WorldMode::Town;
    if (!validMode)
    {
        MG_LOG_E("Invalid world mode");
        MG_ASSERT(validMode && "Invalid world mode");
        return;
    }
    m_ecs   = WorldECS{};
    m_setup = std::move(setup);
    m_ecs.singleton<RngState>().random.seed(m_setup.seed);
    m_ecs.singleton<MapRegion>().mapKey = m_setup.mapKey;
    if (!m_setup.mapKey.empty())
        m_ecs.singleton<MapRegion>().load(
            m_setup.mapKey.ends_with(".layer") ? m_setup.mapKey : "mugen/map/" + m_setup.mapKey + ".layer");
    m_ecs.singleton<RoomState>().roomId = m_setup.roomId;
    m_hashSequence                      = 0;
    m_initialized                       = true;
}

// 创建实体并附加实体组组件，供固定组顺序更新。
Entity World::create(GroupKind kind)
{
    const bool validGroup = kind < GroupKind::Count;
    if (!validGroup)
    {
        MG_LOG_E("Invalid entity group");
        MG_ASSERT(validGroup && "Invalid entity group");
        return {};
    }
    const Entity entity = m_ecs.create();
    m_ecs.add(entity, EntityGroup{kind});
    m_spawnSeq[entity] = m_nextSpawnSeq++;
    return entity;
}

// 将分组实体标记为删除，普通实体直接委托 ECS 销毁。
void World::destroy(Entity entity)
{
    if (auto* group = m_ecs.tryGet<EntityGroup>(entity))
        group->destroyRequested = true;
    else
        m_ecs.destroy(entity);
}

// 使用当前帧和递增序列记录表现层消费事件。
void World::emit(Entity entity, PresentationPayload payload)
{
    auto& events = m_ecs.singleton<PresentationEvents>();
    events.events.push_back({m_ecs.singleton<WorldClock>().frame, entity, events.nextSeq++, std::move(payload)});
}

// 按确定输入顺序应用木偶、系统和玩家控制事件。
void World::applyInput(const FrameInput& input)
{
    auto& room = m_ecs.singleton<RoomState>();
    room.reviveRequests.clear();
    room.puppetUpdates = input.puppetUpdates;
    for (const auto& update : input.puppetUpdates)
    {
        if (!m_ecs.alive(update.entity) || !m_ecs.has<Puppet>(update.entity))
        {
            MG_LOG_W("Ignoring puppet update for missing puppet entity {}", update.entity.value);
            continue;
        }
        auto& transform    = m_ecs.get<Transform>(update.entity);
        transform.position = update.position;
        transform.towardX  = update.towardX < 0 ? -1 : 1;
        auto& motion       = m_ecs.get<MotionState>(update.entity);
        motion.motionName  = update.motionName;
        motion.timeMs      = {};
        if (auto* status = m_ecs.tryGet<RoleStatus>(update.entity))
            status->eEntityStatus = update.status;
    }
    for (const auto& command : input.systemCommands)
    {
        switch (command.index())
        {
        case 0:
            room.reviveRequests.push_back(std::get<ReviveCommand>(command));
            break;
        case 1:
            room.paused = std::get<PauseCommand>(command).paused;
            break;
        case 2:
        {
            const auto& slow              = std::get<SlowMotionCommand>(command);
            m_ecs.singleton<SlowMotion>() = {slow.scale, slow.frames, {}};
            break;
        }
        }
    }
    const auto& slots = m_ecs.singleton<PlayerSlots>().entities;
    for (const auto& player : input.players)
    {
        const auto slot = slots.find(player.slot);
        if (slot == slots.end())
            continue;
        if (room.m_enabled)
        {
            const auto* group = m_ecs.tryGet<EntityGroup>(slot->second);
            if (group && group->suspended) continue;
        }
        if (auto* queue = m_ecs.tryGet<ControlEventQueue>(slot->second))
            for (const auto& command : player.commands)
                if (command.controlEvent != ControlEvent::CTR_NONE)
                    queue->commands.push_back(command);
    }
}

// 捕获指定组的当前成员上界，更新并执行其移除回调。
void World::updateGroup(GroupKind kind)
{
    // Capture each group's upper bound when that group starts, not at the start of the frame.
    std::vector<Entity> members;
    m_ecs.each<EntityGroup>([&](Entity entity, const EntityGroup& group) {
        if (group.kind == kind)
            members.push_back(entity);
    });
    const auto index = static_cast<size_t>(kind);
    size_t cursor    = 0;
    while (cursor < members.size())
    {
        const Entity entity = members[cursor];
        const auto group    = m_ecs.get<EntityGroup>(entity);
        if (!group.suspended && !group.stopped && !m_ecs.singleton<RoomState>().paused)
            m_hooks.update[index](*this, entity);
        // Hooks may grow a component pool: reacquire rather than retaining a reference.
        if (m_ecs.get<EntityGroup>(entity).destroyRequested)
        {
            buff::destroyBuff(*this, entity);
            m_hooks.remove[index](*this, entity);
            std::erase_if(m_ecs.singleton<PlayerSlots>().entities,
                          [entity](const auto& slot) { return slot.second == entity; });
            m_ecs.singleton<LogicTimers>().cancelOwner(entity);
            m_ecs.remove<EntityGroup>(entity);
            m_ecs.destroy(entity);
            members.erase(members.begin() + static_cast<std::ptrdiff_t>(cursor));
        }
        else
            ++cursor;
    }
}

// 校验帧号和输入约束后按固定流水线推进一帧。
void World::tick(const FrameInput& input)
{
    auto& clock          = m_ecs.singleton<WorldClock>();
    const bool nextFrame = m_initialized && input.frame == clock.frame + 1;
    if (!nextFrame)
    {
        MG_LOG_E("FrameInput frame must be the next world frame");
        MG_ASSERT(nextFrame && "FrameInput frame must be the next world frame");
        return;
    }
    const bool permitsPuppetUpdates = m_setup.mode == WorldMode::Town || input.puppetUpdates.empty();
    if (!permitsPuppetUpdates)
    {
        MG_LOG_E("Puppet updates require Town mode");
        MG_ASSERT(permitsPuppetUpdates && "Puppet updates require Town mode");
        return;
    }
    for (const auto& command : input.systemCommands)
        if (const auto* slow = std::get_if<SlowMotionCommand>(&command); slow && slow->scale < Fixed{})
        {
            MG_LOG_E("Slow motion scale must be nonnegative");
            MG_ASSERT(false && "Slow motion scale must be nonnegative");
            return;
        }

    ++clock.frame;
    m_ecs.singleton<PresentationEvents>().events.clear();
    updateLogicTimers(*this);
    applyInput(input);
    room::commands(*this, input);
    m_hooks.camera(*this);
    auto& slow        = m_ecs.singleton<SlowMotion>();
    const bool koActive = slow.m_ko.m_active;
    if (koActive)
    {
        slow.m_ko.update(kLogicDtSec);
        slow.scale = slow.m_ko.m_scale;
    }
    const Fixed scale = koActive || slow.remainingFrames != 0 ? slow.scale : Fixed::fromInt(1);
    if (slow.remainingFrames != 0)
        --slow.remainingFrames;
    clock.dtSec = m_ecs.singleton<RoomState>().paused ? Fixed{} : kLogicDtSec * scale;
    clock.dtMs  = m_ecs.singleton<RoomState>().paused ? Fixed{} : kLogicDtMs * scale;
    clock.elapsedMs += clock.dtMs;
    for (GroupKind kind : kGroupOrder)
        updateGroup(kind);
    m_hooks.scene(*this);
    room::update(*this);
    m_hooks.collision(*this);
    m_ecs.flushDestroyed();
    const auto transfer = m_ecs.singleton<RoomState>();
    if (transfer.m_transferRoom > 0)
        changeRoom(transfer.m_transferRoom, transfer.m_transferPosition, transfer.m_transferToward);
    ++m_hashSequence;
    if (m_trace)
        trace::emitFrame(*m_trace, *this);
}

// 保留实体句柄和池状态，仅回收旧房间拥有的实体与定时器。
bool World::changeRoom(int32_t id, FixedVec3 position, int32_t toward)
{
    const auto* config = conf::Config::instance().find<conf::RoomConfig>(id);
    auto previous = m_ecs.singleton<RoomState>();
    m_ecs.singleton<RoomState>().m_transferRoom = 0;
    if (!config || !previous.m_enabled || m_ecs.singleton<PlayerSlots>().entities.empty()) return false;
    std::vector<Entity> removed;
    std::vector<Entity> retained;
    m_ecs.each<EntityGroup>([&](Entity entity, const EntityGroup& group) {
        bool keep = false;
        if (group.kind == GroupKind::Role)
        {
            const auto* identity = m_ecs.tryGet<RoleIdentity>(entity);
            if (identity)
            {
                const auto* row = conf::Config::instance().find<conf::RoleConfig>(identity->roleId);
                keep = identity->playerSlot.has_value() || (row && row->isPassRoom == 1);
            }
            for (const auto& [slot, player] : m_ecs.singleton<PlayerSlots>().entities)
                keep |= player == entity;
        }
        if (!keep) removed.push_back(entity);
        else retained.push_back(entity);
    });
    for (const auto entity : retained)
    {
        if (auto* brain = m_ecs.tryGet<RoleBrain>(entity))
        {
            if (brain->m_phase == role::brain::BrainPhase::Running)
                role::brain::exitBranch(*brain, *this, entity);
            brain->m_phase             = role::brain::BrainPhase::NotEntered;
            brain->m_detached.m_active = false;
        }
        if (auto* control = m_ecs.tryGet<RoleControl>(entity))
            control->m_last = {};
        if (m_ecs.has<Displacement>(entity)) displacement::reset(*this, entity);
        if (auto* status = m_ecs.tryGet<RoleStatus>(entity))
        {
            status->eEntityStatus = EntityRoleStatus::Idle;
            status->entityParams = {};
        }
        if (auto* queue = m_ecs.tryGet<ControlEventQueue>(entity)) queue->commands.clear();
        if (auto* collider = m_ecs.tryGet<ColliderState>(entity))
        { collider->attack.clear(); collider->damage.clear(); }
    }
    for (const auto entity : removed)
    {
        const auto kind = m_ecs.get<EntityGroup>(entity).kind;
        buff::destroyBuff(*this, entity);
        m_hooks.remove[static_cast<size_t>(kind)](*this, entity);
        m_ecs.singleton<LogicTimers>().cancelOwner(entity);
        m_ecs.destroy(entity);
    }
    m_ecs.flushDestroyed();
    if (previous.m_waveTimer != 0) m_ecs.singleton<LogicTimers>().cancel(previous.m_waveTimer);
    m_ecs.singleton<RoomState>() = {};
    auto& state = m_ecs.singleton<RoomState>();
    state.m_clearedRooms = std::move(previous.m_clearedRooms);
    state.m_usedRevives = previous.m_usedRevives;
    m_ecs.singleton<SlowMotion>() = {};
    m_setup.roomId = id;
    m_setup.mapKey = config->mapKey;
    m_ecs.singleton<MapRegion>() = {};
    m_ecs.singleton<MapRegion>().mapKey = config->mapKey;
    if (!config->mapKey.empty()) m_ecs.singleton<MapRegion>().load("mugen/map/" + config->mapKey + ".layer");
    m_ecs.each<CollisionState>([](Entity, CollisionState& collision) { collision.contacts.clear(); });
    if (!room::enter(*this, *config, previous.m_stageId, previous.m_copyId)) return false;
    for (const auto& [slot, entity] : m_ecs.singleton<PlayerSlots>().entities)
    {
        auto& transform = m_ecs.get<Transform>(entity);
        transform.position = position;
        transform.towardX = toward < 0 ? -1 : 1;
        transform.vectorX = Fixed::fromInt(transform.towardX);
    }
    return true;
}

// 写出版本、房间设置、初始化标志及 ECS 快照。
void World::serialize(ByteBuffer& buffer) const
{
    BinaryWriter writer(buffer);
    writer.write(uint32_t{1});
    writer.write(m_setup);
    writer.write(m_initialized);
    m_ecs.serialize(buffer);
}

// 仅在完整有效快照通过校验时提交恢复后的世界。
bool World::deserialize(ByteBuffer& buffer)
{
    World restored(m_hooks);
    BinaryReader reader(buffer);
    uint32_t version = 0;
    if (!reader.read(version) || version != 1 || !reader.read(restored.m_setup) ||
        !reader.read(restored.m_initialized) || !restored.m_ecs.deserialize(buffer) ||
        buffer.getPosition() != buffer.getSize() ||
        (restored.m_setup.mode != WorldMode::Battle && restored.m_setup.mode != WorldMode::Town))
        return false;
    bool valid = true;
    restored.m_ecs.each<EntityGroup>([&](Entity, const EntityGroup& group) { valid &= group.kind < GroupKind::Count; });
    if (!valid)
        return false;
    restored.m_hashSequence = restored.m_ecs.singleton<WorldClock>().frame;
    *this                   = std::move(restored);
    return true;
}

// 复制本场全部可序列化状态和驱动回调策略；副本不共享语义追踪输出。
World World::clone() const
{
    World copy = *this;
    copy.m_trace = nullptr;
    return copy;
}
// 对规范序列化的世界状态计算 xxh64 哈希。
uint64_t World::hash() const
{
    ByteBuffer buffer;
    serialize(buffer);
    return xxh64(buffer.data(), buffer.len());
}
}  // namespace mugen::sim
