// [1.1] InputSystem
// 读：`FrameInput`、`PlayerSlots`
// 写：`PlayerControlComponent.commands`；木偶的 `TransformComponent`/`MotionStateComponent`/`RoleStateComponent` 请求位
// 消费：—
// 产生：`RoomRequest`（把系统命令转成房间请求入队）
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include <variant>

namespace mugen::sim
{
namespace
{
// 把一帧木偶同步写成变换、动作和 FSM 请求。RoleStateComponent 本身只由 RoleFsmSystem 改。
void applyPuppet(World& world, const PuppetUpdate& update)
{
    auto& ecs = world.ecs();
    if (!ecs.alive(update.entity) || !ecs.has<RoleStateComponent>(update.entity))
        return;
    if (auto* transform = ecs.tryGet<TransformComponent>(update.entity))
    {
        transform->position = update.position;
        transform->towardX  = update.towardX < 0 ? -1 : 1;
    }
    if (auto* motion = ecs.tryGet<MotionStateComponent>(update.entity))
    {
        if (motion->motionName != update.motionName)
        {
            motion->motionName = update.motionName;
            motion->timeMs     = {};
        }
        motion->loop    = true;
        motion->playing = true;
    }
    auto& state             = ecs.get<RoleStateComponent>(update.entity);
    state.puppetState     = role::stateFromStatus(update.status);
    state.puppetRequested = true;
}
}  // namespace

// 系统 InputSystem：1.1 行，见 DESIGN.md §3.2。把帧输入命令按槽位压入 PlayerControlComponent。
void runInput(World& world)
{
    const auto& input = world.input();
    auto& slots       = world.ecs().singleton<PlayerSlots>().entities;
    for (const auto& update : input.puppetUpdates)
        applyPuppet(world, update);
    for (const auto& player : input.players)
    {
        const auto found = slots.find(player.slot);
        if (found == slots.end())
            continue;
        const Entity entity = found->second;
        if (auto* control = world.ecs().tryGet<PlayerControlComponent>(entity))
            for (const auto& command : player.commands)
                if (command.controlEvent != ControlEvent::CTR_NONE)
                    control->commands.push_back(command);
    }
    // 房间级系统命令转成 RoomRequest，由 RoomDirectorSystem 统一消费（移植旧实现 World::tick 的分发）。
    auto& requests = world.ecs().singleton<EventQueue<RoomRequest>>();
    for (const auto& command : input.systemCommands)
    {
        if (const auto* revive = std::get_if<ReviveCommand>(&command))
            requests.push({RoomRequestKind::Revive, static_cast<int32_t>(revive->slot)});
        else if (const auto* back = std::get_if<ReturnToTownCommand>(&command))
            requests.push({RoomRequestKind::ReturnToTown, static_cast<int32_t>(back->slot)});
        else if (const auto* done = std::get_if<RoomWarningDone>(&command))
            requests.push({RoomRequestKind::WarningDone, done->roomId});
    }
}
}  // namespace mugen::sim
