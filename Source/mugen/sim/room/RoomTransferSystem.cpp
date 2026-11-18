// [12.1] RoomTransferSystem
// 读：`RoomState` 换房请求、`PortalStateComponent`(Transfer 阶段)
// 写：整个世界（保留玩家与跨房角色）、`RoomState`(换房请求)
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/sim/role/RoleState.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
// 系统 RoomTransferSystem：12.1 行，见 DESIGN.md §3.2。按传送门请求回收旧房间并进入新房间。
void runRoomTransfer(World& world)
{
    auto& room = world.ecs().singleton<RoomState>();
    // 消费上一帧写入的换房请求。传送门进入 Transfer 阶段由 InteractSystem 写入，本系统在下一帧读到，
    // 复刻旧实现「Portal 组更新先于碰撞」造成的一帧延迟。
    const int32_t target = room.transferRoom;
    room.transferRoom  = 0;
    if (target > 0)
    {
        const auto* config = conf::Config::instance().find<conf::RoomConfig>(target);
        if (config && room.enabled && !world.ecs().singleton<PlayerSlots>().entities.empty())
        {
            // 保留玩家与跨房角色，其余实体回收。
            std::vector<Entity> removed;
            std::vector<Entity> retained;
            world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
                bool keep = false;
                if (identity.kind == SpawnKind::Role)
                {
                    const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
                    keep            = identity.playerSlot.has_value() || (row && row->isPassRoom == 1);
                }
                for (const auto& [slot, player] : world.ecs().singleton<PlayerSlots>().entities)
                    keep = keep || player == entity;
                (keep ? retained : removed).push_back(entity);
            });
            for (const Entity entity : retained)
            {
                if (auto* state = world.ecs().tryGet<RoleStateComponent>(entity))
                {
                    state->state   = RoleStateId::Idle;
                    state->prev    = RoleStateId::Idle;
                    state->stateMs = {};
                }
                if (auto* intent = world.ecs().tryGet<IntentComponent>(entity))
                    intent->move = MoveIntent{};
                if (world.ecs().has<DisplacementComponent>(entity))
                    displacement::rules::reset(world, entity);
            }
            for (const Entity entity : removed)
                world.ecs().destroy(entity);
            world.ecs().flushDestroyed();

            // 复位房间状态并进入新房间。
            const std::vector<int32_t> cleared = std::move(room.clearedRooms);
            const int32_t usedRevives          = room.usedRevives;
            const int32_t stageId              = room.stageId;
            const int32_t copyId               = room.copyId;
            const FixedVec3 position           = room.transferPosition;
            const int32_t toward               = room.transferToward;
            room                               = RoomState{};
            room.clearedRooms                = cleared;
            room.usedRevives                 = usedRevives;
            world.ecs().singleton<SlowMotion>()  = {};
            world.ecs().singleton<MapRegion>()   = {};
            world.ecs().singleton<MapRegion>().mapKey = config->mapKey;
            if (!config->mapKey.empty())
                world.ecs().singleton<MapRegion>().load("/mugen/map/" + config->mapKey + ".layer");

            auto& spawnEvents            = world.ecs().singleton<EventQueue<SpawnEvent>>();
            const size_t beforeSpawn     = spawnEvents.items.size();
            room::rules::enter(world, *config, stageId, copyId);
            for (const auto& [slot, entity] : world.ecs().singleton<PlayerSlots>().entities)
            {
                auto& transform     = world.ecs().get<TransformComponent>(entity);
                transform.position = position;
                transform.towardX  = toward < 0 ? -1 : 1;
                transform.vectorX  = Fixed::fromInt(transform.towardX);
            }
            // 本系统晚于 DebugSink（P11），换房新生成的角色补发 spawn 追踪行，供摘要导出。
            for (size_t i = beforeSpawn; i < spawnEvents.items.size(); ++i)
            {
                const SpawnEvent& event = spawnEvents.items[i];
                if (event.kind != SpawnKind::Role && event.kind != SpawnKind::Effect)
                    continue;
                const char* stem = event.kind == SpawnKind::Role ? "role" : "fx";
                world.appendTrace("spawn - " + std::string(stem) + ':' + std::to_string(event.configId) + '#' +
                                  std::to_string(event.ordinal));
            }
        }
    }
    // 本帧进入 Transfer 阶段的传送门写入下一帧的换房请求（对齐旧实现一帧延迟）。
    world.ecs().each<PortalStateComponent>([&](Entity, const PortalStateComponent& state) {
        if (room.transferRoom == 0 && state.phase == PortalPhase::Transfer)
        {
            room.transferRoom     = state.roomId;
            room.transferPosition = state.destination;
            room.transferToward   = state.toward;
        }
    });
}
}  // namespace mugen::sim
