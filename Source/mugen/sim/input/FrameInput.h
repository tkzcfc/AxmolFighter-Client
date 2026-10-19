#pragma once
#include "PlayerCommand.h"
#include "mugen/core/ecs/Entity.h"

namespace mugen::sim
{
// 单个玩家槽位在一帧内提交的命令。
struct PlayerInput
{
    // 输入所属的玩家槽位。
    uint32_t slot = 0;
    // 保持提交顺序的控制命令。
    std::vector<PlayerCommand> commands;
    // 参与帧输入归档的玩家字段。
    MG_REFLECT(PlayerInput, slot, commands)
};
// 请求复活指定玩家槽位。
struct ReviveCommand
{
    // 要复活的玩家槽位。
    uint32_t slot = 0;
    // 参与系统命令归档的字段。
    MG_REFLECT(ReviveCommand, slot)
};
// 请求暂停或继续世界。
struct PauseCommand
{
    // 目标暂停状态。
    bool paused = true;
    // 参与系统命令归档的字段。
    MG_REFLECT(PauseCommand, paused)
};
// 请求指定时长的慢动作。
struct SlowMotionCommand
{
    // 逻辑时间缩放倍率。
    Fixed scale = Fixed::fromInt(1);
    // 缩放持续的逻辑帧数。
    uint32_t frames = 0;
    // 参与系统命令归档的字段。
    MG_REFLECT(SlowMotionCommand, scale, frames)
};
// 一帧可提交的房间级系统命令。
struct ReturnToTownCommand
{
    // 返回命令所属玩家槽位。
    uint32_t m_slot = 0;
    MG_REFLECT(ReturnToTownCommand, m_slot)
};
// 房间警告展示结束后由驱动提交确认。
struct RoomWarningDone
{
    // 所属房间，拒绝过期确认。
    int32_t m_roomId = 0;
    MG_REFLECT(RoomWarningDone, m_roomId)
};
using SystemCommand =
    std::variant<ReviveCommand, PauseCommand, SlowMotionCommand, ReturnToTownCommand, RoomWarningDone>;
// 外部控制角色在一帧内的同步状态。
struct PuppetUpdate
{
    // 需要同步的实体。
    Entity entity;
    // 目标世界坐标。
    FixedVec3 position;
    // 目标朝向，仅取 -1 或 1。
    int32_t towardX = 1;
    // 目标角色状态位。
    EntityRoleStatus status = EntityRoleStatus::None;
    // 目标动作名称。
    std::string motionName;
    // 参与帧输入归档的受控角色字段。
    MG_REFLECT(PuppetUpdate, entity, position, towardX, status, motionName)
};
// 驱动世界推进一次的完整确定性输入。
struct FrameInput
{
    // 该输入对应的逻辑帧号。
    uint64_t frame = 0;
    // 各玩家槽位提交的命令。
    std::vector<PlayerInput> players;
    // 本帧的房间级命令。
    std::vector<SystemCommand> systemCommands;
    // 本帧的受控角色同步数据。
    std::vector<PuppetUpdate> puppetUpdates;
    // 参与回放和快照的帧字段。
    MG_REFLECT(FrameInput, frame, players, systemCommands, puppetUpdates)
};
}  // namespace mugen::sim
