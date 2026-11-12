#include "mugen/sim/Schedule.h"
#include "mugen/sim/Systems.h"

namespace mugen::sim
{
namespace
{
// 调度表本体，顺序就是 DESIGN.md §3.2。
constexpr SystemEntry kScheduleRows[] = {
    {"ClockSystem", static_cast<uint8_t>(Phase::FrameBegin), runClock},
    {"ScheduledActionSystem", static_cast<uint8_t>(Phase::FrameBegin), runScheduledAction},
    {"InputSystem", static_cast<uint8_t>(Phase::Input), runInput},
    {"CooldownSystem", static_cast<uint8_t>(Phase::Tick), runCooldown},
    {"BuffTickSystem", static_cast<uint8_t>(Phase::Tick), runBuffTick},
    {"VitalsTickSystem", static_cast<uint8_t>(Phase::Tick), runVitalsTick},
    {"PauseTickSystem", static_cast<uint8_t>(Phase::Tick), runPauseTick},
    {"AiSystem", static_cast<uint8_t>(Phase::Intent), runAi},
    {"PlayerIntentSystem", static_cast<uint8_t>(Phase::Intent), runPlayerIntent},
    {"SkillRequestSystem", static_cast<uint8_t>(Phase::Action), runSkillRequest},
    {"RoleFsmSystem", static_cast<uint8_t>(Phase::Action), runRoleFsm},
    {"TimelineSystem(roles)", static_cast<uint8_t>(Phase::Action), runTimelineRoles},
    {"SpawnFlushSystem(RoleTimeline)", static_cast<uint8_t>(Phase::Action), runSpawnFlushRoleTimeline},
    {"TimelineSystem(effects)", static_cast<uint8_t>(Phase::Action), runTimelineEffects},
    {"SpawnFlushSystem(EffectTimeline)", static_cast<uint8_t>(Phase::Action), runSpawnFlushEffectTimeline},
    {"HitReactionSystem", static_cast<uint8_t>(Phase::Action), runHitReaction},
    {"LocomotionSystem", static_cast<uint8_t>(Phase::Action), runLocomotion},
    {"DisplacementSystem", static_cast<uint8_t>(Phase::Movement), runDisplacement},
    {"FollowSystem", static_cast<uint8_t>(Phase::Movement), runFollow},
    {"AnimationSystem", static_cast<uint8_t>(Phase::Animation), runAnimation},
    {"HitboxSystem", static_cast<uint8_t>(Phase::Animation), runHitbox},
    {"CollisionSystem", static_cast<uint8_t>(Phase::Collision), runCollision},
    {"CombatSystem", static_cast<uint8_t>(Phase::Combat), runCombat},
    {"ObstacleSystem", static_cast<uint8_t>(Phase::Combat), runObstacle},
    {"InteractSystem", static_cast<uint8_t>(Phase::Combat), runInteract},
    {"SpawnFlushSystem(Combat)", static_cast<uint8_t>(Phase::Combat), runSpawnFlushCombat},
    {"DeathSystem", static_cast<uint8_t>(Phase::Lifecycle), runDeath},
    {"TransformSystem", static_cast<uint8_t>(Phase::Lifecycle), runTransform},
    {"DespawnSystem", static_cast<uint8_t>(Phase::Lifecycle), runDespawn},
    {"SpawnFlushSystem(Lifecycle)", static_cast<uint8_t>(Phase::Lifecycle), runSpawnFlushLifecycle},
    {"RoomDirectorSystem", static_cast<uint8_t>(Phase::Room), runRoomDirector},
    {"SpawnFlushSystem(Room)", static_cast<uint8_t>(Phase::Room), runSpawnFlushRoom},
    {"PresentationSystem", static_cast<uint8_t>(Phase::Present), runPresentation},
    {"DebugSinkSystem", static_cast<uint8_t>(Phase::Present), runDebugSink},
    {"RoomTransferSystem", static_cast<uint8_t>(Phase::FrameEnd), runRoomTransfer},
    {"FlushSystem", static_cast<uint8_t>(Phase::FrameEnd), runFlush},
};
}  // namespace

const std::span<const SystemEntry> kSchedule{kScheduleRows};
}  // namespace mugen::sim
