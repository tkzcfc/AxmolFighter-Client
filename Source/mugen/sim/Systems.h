#pragma once
#include "mugen/sim/base/SpawnStage.h"

namespace mugen::sim
{
// 声明世界类型，系统入口只依赖它。
class World;

// 全部系统入口，对应 DESIGN.md §3.2 调度表的每一行。
// [0.1] ClockSystem
void runClock(World&);
// [0.2] ScheduledActionSystem
void runScheduledAction(World&);
// [1.1] InputSystem
void runInput(World&);
// [2.1] CooldownSystem
void runCooldown(World&);
// [2.2] BuffTickSystem
void runBuffTick(World&);
// [2.3] VitalsTickSystem
void runVitalsTick(World&);
// [2.4] PauseTickSystem
void runPauseTick(World&);
// [3.1] AiSystem
void runAi(World&);
// [3.2] PlayerIntentSystem
void runPlayerIntent(World&);
// [4.1] SkillRequestSystem
void runSkillRequest(World&);
// [4.2] RoleFsmSystem
void runRoleFsm(World&);
// [4.3a] TimelineSystem(roles)
void runTimelineRoles(World&);
// [4.3s] SpawnFlush(RoleTimeline)
void runSpawnFlushRoleTimeline(World&);
// [4.3b] TimelineSystem(effects)
void runTimelineEffects(World&);
// [4.3t] SpawnFlush(EffectTimeline)
void runSpawnFlushEffectTimeline(World&);
// [4.4] HitReactionSystem
void runHitReaction(World&);
// [4.5] LocomotionSystem
void runLocomotion(World&);
// [5.1] DisplacementSystem
void runDisplacement(World&);
// [5.2] FollowSystem
void runFollow(World&);
// [6.1] AnimationSystem
void runAnimation(World&);
// [6.2] HitboxSystem
void runHitbox(World&);
// [7.1] CollisionSystem
void runCollision(World&);
// [8.1] CombatSystem
void runCombat(World&);
// [8.2] ObstacleSystem
void runObstacle(World&);
// [8.3] InteractSystem
void runInteract(World&);
// [8.s] SpawnFlush(Combat)
void runSpawnFlushCombat(World&);
// [9.1] DeathSystem
void runDeath(World&);
// [9.2] TransformSystem
void runTransform(World&);
// [9.3] DespawnSystem
void runDespawn(World&);
// [9.s] SpawnFlush(Lifecycle)
void runSpawnFlushLifecycle(World&);
// [10.1] RoomDirectorSystem
void runRoomDirector(World&);
// [10.s] SpawnFlush(Room)
void runSpawnFlushRoom(World&);
// [11.1] PresentationSystem
void runPresentation(World&);
// [11.2] DebugSink
void runDebugSink(World&);
// [12.1] RoomTransferSystem
void runRoomTransfer(World&);
// [12.2] FlushSystem
void runFlush(World&);
}  // namespace mugen::sim
