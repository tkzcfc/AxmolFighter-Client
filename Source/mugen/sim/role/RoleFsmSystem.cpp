// [4.2] RoleFsmSystem
// 读：`RoleStateComponent`（含木偶请求）、`SkillCastComponent`、`IntentComponent.move`、`HitStateComponent`、`HealthComponent`、`DisplacementComponent` 事件位、`MotionStateComponent` 完成位
// 写：`RoleStateComponent`（工厂初始化之外的唯一写入者）；进入/退出动作写 `MotionStateComponent`、`DisplacementComponent`、`SkillCastComponent`、`ActionTimelineComponent`（经 skill::rules）；复活写 `HealthComponent`（回血、解除死亡）
// 消费：—
// 产生：`StateEvent`、`CastEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleFsmRules.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/motion/DisplacementRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/battle/HitDef.h"
#include "mugen/conf/battle/RoleDef.h"

namespace mugen::sim
{
namespace
{
// 查询角色静态配置（身份有效时不为空）。
const conf::battle::RoleDef* roleDef(const IdentityComponent& identity)
{
    return conf::Config::instance().find<conf::battle::RoleDef>(identity.configId);
}
// 查询受击配置。
const conf::battle::HitDef* hitDef(int32_t id)
{
    return id > 0 ? conf::Config::instance().find<conf::battle::HitDef>(id) : nullptr;
}
// 浮空值耗尽判定：运行值耗尽且配置值有效才算触发浮空保护。
bool isRigidity(const HitStateComponent& hit, const conf::battle::RoleDef* role)
{
    return role && hit.rigidity <= Fixed{} && Fixed::fromInt(role->rigidity) > Fixed{};
}
// 硬直/倒地时长修正（AddFloorTime = 扩展属性 115）。
Fixed floorTimeScale(World& world, Entity entity)
{
    return Fixed::fromInt(1) + world.ecs().get<StatsComponent>(entity).finalStat(115);
}
// 读取硬直：浮空值耗尽且未死时逐次扣除疲劳。
Fixed getStiffTime(HitStateComponent& hit, const conf::battle::RoleDef* role, bool dead)
{
    if (!dead && isRigidity(hit, role))
        hit.stiffTime = std::max(Fixed{}, hit.stiffTime - hit.fatigue * Fixed::fromInt(hit.hitCounts));
    return hit.stiffTime;
}
// 是否持有指定 Buff（跳过已标记移除的实例）。
bool hasBuff(World& world, Entity entity, int32_t id)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return false;
    for (const auto& inst : world.ecs().get<BuffListComponent>(entity).instances)
        if (!inst.removing && inst.id == id)
            return true;
    return false;
}
// 扣浮空值；耗尽后按重量与受击计数加速下落。
void applyWeight(World& world, Entity entity, HitStateComponent& hit, const conf::battle::RoleDef* role, Fixed amount)
{
    hit.rigidity = std::max(Fixed{}, hit.rigidity - amount);
    const bool dead = world.ecs().has<HealthComponent>(entity) && world.ecs().get<HealthComponent>(entity).dead;
    if (dead || !isRigidity(hit, role))
        return;
    world.ecs().get<DisplacementComponent>(entity).velocity.y -= hit.weight * Fixed::fromInt(hit.hitCounts);
    const auto& identity = world.ecs().get<IdentityComponent>(entity);
    if (identity.roleType == 1 && !hasBuff(world, entity, 155) && !hasBuff(world, entity, 173))
    {
        buff::rules::admit(world, entity, 155);
        buff::rules::admit(world, entity, 154);
    }
}
// 唯一状态写入：更新状态、清零计时并发出带正确 from 的 StateEvent。
using role::rules::transition;
// 播放受击族动作（除 Wake 外均不循环）。
void playHitMotion(MotionStateComponent& motion, RoleStateId state)
{
    motion.motionName         = role::motionName(state);
    motion.timeMs             = {};
    motion.completedLoops     = 0;
    motion.completedThisFrame = false;
    motion.entryId.clear();
    motion.loop               = false;
    motion.rate               = state == RoleStateId::HitUp      ? conf::kHitUpScaleTime
                                  : state == RoleStateId::HitDown  ? conf::kHitDownScaleTime
                                  : state == RoleStateId::HitFloor ? conf::kHitFloorScaleTime
                                  : state == RoleStateId::GetUp    ? conf::kGetUpScaleTime
                                                                    : conf::kHitScaleTime;
    motion.playing            = true;
}
// 播放待机动作。
using role::rules::playIdle;
// 启动受击位移。Began 由 displacement::rules::set 发出；缺失位移行时回退为 -1 避免永久等制动。
void setHitDisplacement(World& world, Entity entity, HitStateComponent& hit)
{
    if (hit.displacementId != -1 && !displacement::rules::set(world, entity, hit.displacementId))
        hit.displacementId = -1;
}
// 进入受击（首次受击 first=true 时重设受击计数）。位移标识由 Combat 已按角色覆盖规则解析好。
void enterHit(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
              bool first)
{
    hit.hitDefId        = hit.pendingHitDefId;
    hit.hitType         = hit.pendingHitType;
    hit.displacementId  = hit.pendingDisplacementId;
    hit.towardX         = hit.pendingTowardX;
    hit.pendingHitDefId = 0;
    hit.animationEnd    = false;
    hit.displacementEnd = false;
    hit.hitTimeMs       = {};
    if (first)
        hit.hitCounts = 1;
    playHitMotion(motion, RoleStateId::Hit);
    setHitDisplacement(world, entity, hit);
    state.stateMs = {};
}
// 进入浮空：保留进行中的位移曲线（set(-1) 不清速度），只重播动作。
void enterHitAir(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                 RoleStateId stateId)
{
    (void)world;
    (void)entity;
    hit.animationEnd    = false;
    hit.displacementEnd = false;
    hit.hitTimeMs       = {};
    playHitMotion(motion, stateId);
    state.stateMs = {};
}
// 进入死亡：清施法、播放死亡动作并启动按路径决定的死亡位移（-1/0 表示无死亡位移）。
// 死亡位移查不到时按无位移处理，避免一直等待不会到来的落地事件。
void enterDeath(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                DisplacementComponent& disp, const IdentityComponent& identity, int32_t deathDisplacementId)
{
    skill::rules::forceInterrupt(world, entity);
    motion.motionName         = role::motionName(RoleStateId::Death);
    motion.timeMs             = {};
    motion.loop               = false;
    motion.completedLoops     = 0;
    motion.completedThisFrame = false;
    motion.entryId.clear();
    motion.playing            = true;
    hit.animationEnd          = false;
    hit.displacementEnd   = false;
    hit.pendingHitDefId   = 0;
    hit.displacementId    = deathDisplacementId;
    disp.brake            = false;
    disp.lie              = false;
    state.stateMs         = {};
    (void)identity;
    if (!displacement::rules::set(world, entity, deathDisplacementId))
        hit.displacementId = -1;
}
// 进入起身：英雄挂起身保护 156 与白框 154。
void enterGetUp(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                const IdentityComponent& identity)
{
    hit.animationEnd    = false;
    hit.displacementEnd = false;
    hit.hitTimeMs       = {};
    playHitMotion(motion, RoleStateId::GetUp);
    if (identity.roleType == 1)
    {
        buff::rules::admit(world, entity, 156);
        buff::rules::admit(world, entity, 154);
    }
    state.stateMs = {};
}
// 进入醒来：播放待机，恢复浮空值，英雄挂无敌 201。
void enterWake(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
               const IdentityComponent& identity)
{
    playIdle(motion);
    if (const auto* role = roleDef(identity))
        hit.rigidity = Fixed::fromInt(role->rigidity);
    const int32_t wakeBuff = identity.roleType == 1 ? conf::kRoleHeroWakeBuffId : conf::kRoleEnemyWakeBuffId;
    if (wakeBuff != -1)
        buff::rules::admit(world, entity, wakeBuff);
    if (identity.roleType == 1)
    {
        hit.hitProtectHp.reset();
        hit.hitProtectTime     = {};
        hit.hitProtectExecuted = false;
    }
    state.stateMs = {};
}
// 死亡体维持：累计死亡时长，读上一帧动画与位移完成位。
// 落地事件若没送到当前死亡动作，贴地且不再向上时仍视为位移结束。
void updateDeath(World& world, Entity entity, RoleStateComponent& state, const MotionStateComponent& motion,
                 const DisplacementComponent& disp, HitStateComponent& hit, Fixed dtMs)
{
    state.stateMs += dtMs;
    if (motion.completedLoops > 0)
        hit.animationEnd = true;
    if (disp.brake || disp.lie)
        hit.displacementEnd = true;
    if (!hit.displacementEnd && hit.displacementId != -1)
    {
        const auto& transform = world.ecs().get<TransformComponent>(entity);
        if (transform.position.y <= Fixed{} && disp.velocity.y <= Fixed{})
            hit.displacementEnd = true;
    }
}
// 受击自然推进：位移制动后计硬直，满硬直后已死进死亡（套死亡位移），否则进 HitSwitch。
// 返回是否发生了状态转移（追击需在其后处理，原版 dealWithTime 先于 dealWithDoubleHit）。
bool updateHit(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
               DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    const auto* role = roleDef(identity);
    const bool dead  = state.deathRequested;
    // 动画完成：击倒/击飞且角色未配置受击位移覆盖时转入浮空。
    if (motion.completedThisFrame)
    {
        hit.animationEnd = true;
        if (hit.hitType != 0 && role && role->hitDisplacementId == -1)
        {
            transition(world, entity, state, RoleStateId::HitUp);
            enterHitAir(world, entity, state, hit, motion, RoleStateId::HitUp);
            return true;
        }
    }
    if (disp.brake && !hit.displacementEnd)
    {
        hit.displacementEnd = true;
        hit.hitTimeMs       = {};
    }
    else if (disp.air && !hit.displacementEnd)
    {
        hit.displacementEnd = true;
        if (!hit.animationEnd)
        {
            transition(world, entity, state, RoleStateId::HitDown);
            enterHitAir(world, entity, state, hit, motion, RoleStateId::HitDown);
            return true;
        }
    }
    hit.hitTimeMs += dtMs;
    if (identity.roleType == 1 && hit.hitProtectExecuted)
    {
        transition(world, entity, state, RoleStateId::HitDown);
        enterHitAir(world, entity, state, hit, motion, RoleStateId::HitDown);
        return true;
    }
    const bool timed =
        (hit.hitType == 0 || hit.displacementId == -1 || (role && role->hitDisplacementId != -1)) &&
        hit.animationEnd && (hit.displacementEnd || hit.displacementId == -1) &&
        hit.hitTimeMs >= getStiffTime(hit, role, dead);
    if (timed)
    {
        if (dead)
        {
            transition(world, entity, state, RoleStateId::Death);
            enterDeath(world, entity, state, hit, motion, disp, identity, role ? role->deathDisplacementId : -1);
        }
        else
        {
            hit.stiffTime = {};
            displacement::rules::reset(world, entity);
            transition(world, entity, state, RoleStateId::HitSwitch);
            playHitMotion(motion, RoleStateId::HitSwitch);
        }
        return true;
    }
    state.stateMs += dtMs;
    return false;
}
// 受击恢复：上一帧动画完成则回到待机；已死则进入死亡（不套死亡位移）。
void updateHitSwitch(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                     DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    if (!motion.completedThisFrame)
    {
        state.stateMs += dtMs;
        return;
    }
    if (state.deathRequested)
    {
        transition(world, entity, state, RoleStateId::Death);
        enterDeath(world, entity, state, hit, motion, disp, identity, -1);
        return;
    }
    transition(world, entity, state, RoleStateId::Idle);
    playIdle(motion);
}
// 浮空推进：动画完成且位移 air/lie 后转入下落（两者到达先后不限）。
void updateHitUp(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                 DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    (void)identity;
    if (motion.completedThisFrame)
    {
        hit.animationEnd = true;
        if (hit.displacementEnd)
        {
            transition(world, entity, state, RoleStateId::HitDown);
            enterHitAir(world, entity, state, hit, motion, RoleStateId::HitDown);
            return;
        }
    }
    if ((disp.air || disp.lie) && !hit.displacementEnd)
    {
        hit.displacementEnd = true;
        if (hit.animationEnd)
        {
            transition(world, entity, state, RoleStateId::HitDown);
            enterHitAir(world, entity, state, hit, motion, RoleStateId::HitDown);
            return;
        }
    }
    state.stateMs += dtMs;
}
// 下落推进：位移 lie 且动画完成后转入倒地（两者到达先后不限）。
void updateHitDown(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                   DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    (void)identity;
    if (motion.completedThisFrame)
    {
        hit.animationEnd = true;
        if (hit.displacementEnd)
        {
            transition(world, entity, state, RoleStateId::HitFloor);
            enterHitAir(world, entity, state, hit, motion, RoleStateId::HitFloor);
            return;
        }
    }
    if (disp.lie && !hit.displacementEnd)
    {
        hit.displacementEnd = true;
        if (hit.animationEnd)
        {
            transition(world, entity, state, RoleStateId::HitFloor);
            enterHitAir(world, entity, state, hit, motion, RoleStateId::HitFloor);
            return;
        }
    }
    state.stateMs += dtMs;
}
// 倒地推进：动画完成且落地后开始计硬直，满硬直后起身或死亡（倒地死亡不套死亡位移）。
// 返回是否发生了状态转移。
bool updateHitFloor(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                    DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    const auto* role = roleDef(identity);
    const bool dead  = state.deathRequested;
    if (motion.completedThisFrame)
    {
        hit.animationEnd = true;
        hit.hitTimeMs    = {};
        if (hit.displacementEnd && dead)
        {
            transition(world, entity, state, RoleStateId::Death);
            enterDeath(world, entity, state, hit, motion, disp, identity, -1);
            return true;
        }
    }
    if ((disp.brake || disp.lie) && !hit.displacementEnd)
    {
        hit.displacementEnd = true;
        hit.hitTimeMs       = {};
        if (hit.animationEnd && dead)
        {
            transition(world, entity, state, RoleStateId::Death);
            enterDeath(world, entity, state, hit, motion, disp, identity, -1);
            return true;
        }
    }
    hit.hitTimeMs += dtMs;
    if (hit.animationEnd && hit.displacementEnd && hit.hitTimeMs >= getStiffTime(hit, role, dead))
    {
        hit.stiffTime = {};
        if (dead)
        {
            transition(world, entity, state, RoleStateId::Death);
            enterDeath(world, entity, state, hit, motion, disp, identity, -1);
        }
        else
        {
            transition(world, entity, state, RoleStateId::GetUp);
            enterGetUp(world, entity, state, hit, motion, identity);
        }
        return true;
    }
    state.stateMs += dtMs;
    return false;
}
// 起身推进：动画完成后转入醒来（已死则套死亡位移）。
void updateGetUp(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                 DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    if (!motion.completedThisFrame)
    {
        state.stateMs += dtMs;
        return;
    }
    if (state.deathRequested)
    {
        const auto* role = roleDef(identity);
        transition(world, entity, state, RoleStateId::Death);
        enterDeath(world, entity, state, hit, motion, disp, identity, role ? role->deathDisplacementId : -1);
        return;
    }
    transition(world, entity, state, RoleStateId::Wake);
    enterWake(world, entity, state, hit, motion, identity);
}
// 醒来推进：立即回到待机（已死则套死亡位移）。
void updateWake(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                DisplacementComponent& disp, const IdentityComponent& identity, Fixed dtMs)
{
    if (state.deathRequested)
    {
        const auto* role = roleDef(identity);
        transition(world, entity, state, RoleStateId::Death);
        enterDeath(world, entity, state, hit, motion, disp, identity, role ? role->deathDisplacementId : -1);
        return;
    }
    transition(world, entity, state, RoleStateId::Idle);
    playIdle(motion);
    state.stateMs += dtMs;
}
// 地面受击追击（原版 ActionHit:dealWithDoubleHit）：重播或按命中类型转入浮空。
void chaseHitGround(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
                    const conf::battle::RoleDef* role, const conf::battle::HitDef& data)
{
    const bool airborne = world.ecs().get<TransformComponent>(entity).position.y > Fixed{};
    hit.hitDefId       = data.id;
    hit.hitType        = data.hitType;
    hit.towardX        = hit.pendingTowardX;
    hit.pendingHitDefId = 0;
    hit.displacementEnd = false;
    bool toHitUp          = false;
    hit.displacementId  = role ? role->hitDisplacementId : -1;
    if (hit.displacementId == -1)
    {
        hit.displacementId = data.displacementId;
        if (data.hitType == 0)
        {
            if (!airborne)
            {
                playHitMotion(motion, RoleStateId::Hit);
                hit.animationEnd = false;
            }
            else
                hit.displacementId = data.airDisplacementId;
        }
        else if (hit.animationEnd)
            toHitUp = true;
    }
    else
    {
        playHitMotion(motion, RoleStateId::Hit);
        hit.animationEnd = false;
        if (hit.displacementId == 0)
            hit.displacementId = -1;
        else if (data.hitType == 0)
            hit.displacementId = data.displacementId;
    }
    if (!displacement::rules::set(world, entity, hit.displacementId))
        hit.displacementId = -1;
    if (toHitUp)
    {
        transition(world, entity, state, RoleStateId::HitUp);
        enterHitAir(world, entity, state, hit, motion, RoleStateId::HitUp);
    }
}
// 空中/倒地追击（原版 ActionHitUp/Down/Floor:dealWithDoubleHit）。
void chaseAir(World& world, Entity entity, RoleStateComponent& state, HitStateComponent& hit, MotionStateComponent& motion,
              const conf::battle::RoleDef* role, const conf::battle::HitDef& data)
{
    hit.hitDefId        = data.id;
    hit.hitType         = data.hitType;
    hit.towardX         = hit.pendingTowardX;
    hit.pendingHitDefId = 0;
    hit.displacementId  = data.hitType == 0 ? data.airDisplacementId : data.displacementId;
    if (!displacement::rules::set(world, entity, hit.displacementId))
        hit.displacementId = -1;
    if (data.hitType != 1)
        applyWeight(world, entity, hit, role, data.hitRigidity);
    const RoleStateId current = state.state;
    if (current == RoleStateId::HitUp)
    {
        if (data.hitType == 1)
        {
            transition(world, entity, state, RoleStateId::HitDown);
            enterHitAir(world, entity, state, hit, motion, RoleStateId::HitDown);
        }
        if (data.hitType == 0 || data.hitType == 2)
            hit.displacementEnd = false;
        return;
    }
    const bool knockDown = current == RoleStateId::HitFloor && data.hitType == 1;
    const bool relaunch  = (data.hitType == 0 || data.hitType == 2) && !isRigidity(hit, role);
    if (knockDown)
    {
        transition(world, entity, state, RoleStateId::HitDown);
        enterHitAir(world, entity, state, hit, motion, RoleStateId::HitDown);
        hit.stiffTime = Fixed::fromInt(data.stiffTime) * floorTimeScale(world, entity);
    }
    if (relaunch)
    {
        transition(world, entity, state, RoleStateId::HitUp);
        enterHitAir(world, entity, state, hit, motion, RoleStateId::HitUp);
        hit.stiffTime = Fixed::fromInt(data.stiffTime) * floorTimeScale(world, entity);
    }
}
// 施法结束回到待机；Attack 内切招结束旧施法并开始新施法。
void updateAttack(World& world, Entity entity, RoleStateComponent& state, SkillCastComponent& cast, MotionStateComponent& motion,
                  const IntentComponent& intent, Fixed dtMs)
{
    if (cast.switched)
    {
        cast.switched = false;
        skill::rules::endCast(world, entity, true);
        if (cast.skill != kNoSkill)
        {
            state.stateMs += dtMs;
            return;
        }
    }
    if (cast.skill == kNoSkill)
    {
        role::rules::exitAttack(world, entity, state, cast, motion, intent);
        return;
    }
    state.stateMs += dtMs;
}
// 移动 / 待机。
void updateLocomotion(World& world, Entity entity, RoleStateComponent& state, const IntentComponent& intent, MotionStateComponent& motion,
                      DisplacementComponent& disp, Fixed dtMs)
{
    RoleStateId next = state.state;
    if (state.state == RoleStateId::Idle)
    {
        if (intent.move.mode == MoveMode::Run)
            next = RoleStateId::Run;
        else if (intent.move.mode == MoveMode::Walk)
            next = RoleStateId::Walk;
    }
    else if (role::isMoveState(state.state))
    {
        if (intent.move.mode == MoveMode::Stop)
            next = RoleStateId::Idle;
    }
    if (next != state.state)
    {
        transition(world, entity, state, next);
        motion.motionName = role::motionName(next);
        motion.timeMs     = {};
        motion.loop       = true;
        motion.playing    = true;
        motion.rate       = next == RoleStateId::Walk ? conf::kWalkScaleTime
                              : next == RoleStateId::Run ? conf::kRunScaleTime
                                                        : conf::kIdleScaleTime;
        disp.velocity     = {};
    }
    else
        state.stateMs += dtMs;
    if (state.state == RoleStateId::Idle && motion.motionName == role::motionName(RoleStateId::HitSwitch))
        playIdle(motion);
}
}  // namespace

// 系统 RoleFsmSystem：4.2 行，见 DESIGN.md §3.2。唯一写 RoleStateComponent 的系统。
// 受击族状态机按原版语义实现：Hit / HitSwitch / HitUp / HitDown / HitFloor / GetUp / Wake。
void runRoleFsm(World& world)
{
    const Fixed dtMs = world.ecs().singleton<WorldClock>().dtMs;
    world.ecs().each<RoleStateComponent, IntentComponent, MotionStateComponent, DisplacementComponent, SkillCastComponent, HitStateComponent, IdentityComponent>(
        [&](Entity entity, RoleStateComponent& state, const IntentComponent& intent, MotionStateComponent& motion, DisplacementComponent& disp,
            SkillCastComponent& cast, HitStateComponent& hit, const IdentityComponent& identity) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            state.prev = state.state;
            const bool paused = [&]() {
                if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                    return pause->paused;
                return false;
            }();

            // 顿帧期间：状态转移仍进行（进入受击/死亡），但 enter 动作（动画/位移）与一切推进冻结。
            // 原版 dealWithBeHitBegan 先 setFreezeTime 再同帧 dealWithStatus(Hit)，转移不被顿帧阻塞。
            if (paused)
            {
                // 死亡请求优先。
                if (state.deathRequested && !role::isHitState(state.state))
                {
                    transition(world, entity, state, RoleStateId::Death);
                    hit.enterPending = true;
                    return;
                }
                // 首次受击：转移到 Hit，enter 动作推迟到解冻。
                if (hit.pendingHitDefId != 0 && !role::isHitState(state.state))
                {
                    transition(world, entity, state, RoleStateId::Hit);
                    hit.enterPending = true;
                    return;
                }
                return;
            }

            // 解冻后补做被顿帧推迟的 enter 动作。
            if (hit.enterPending)
            {
                hit.enterPending = false;
                if (state.state == RoleStateId::Death)
                {
                    const auto* role = roleDef(identity);
                    enterDeath(world, entity, state, hit, motion, disp, identity, role ? role->deathDisplacementId : -1);
                }
                else
                    enterHit(world, entity, state, hit, motion, true);
                return;
            }

            // 1. 已在死亡：先处理复活请求（Death → Revive → Idle），否则只维持完成位。
            if (state.state == RoleStateId::Death)
            {
                if (state.reviveRequested)
                {
                    state.reviveRequested = false;
                    transition(world, entity, state, RoleStateId::Revive);
                    auto& health = world.ecs().get<HealthComponent>(entity);
                    health.hp   = health.maxHp;
                    health.dead = false;
                    state.deathRequested = false;
                    state.deathAnnounced = false;
                    state.deathKiller    = {};
                    transition(world, entity, state, RoleStateId::Idle);
                    playIdle(motion);
                }
                else
                    updateDeath(world, entity, state, motion, disp, hit, dtMs);
                return;
            }
            // 1b. 死亡请求：非受击族立即进入死亡（套死亡位移）；受击族等自然出口。
            if (state.deathRequested && !role::isHitState(state.state))
            {
                const auto* role = roleDef(identity);
                transition(world, entity, state, RoleStateId::Death);
                enterDeath(world, entity, state, hit, motion, disp, identity, role ? role->deathDisplacementId : -1);
                return;
            }
            // 木偶同步只提交请求，状态由这里写入；城镇远端木偶在未收到新快照帧保持当前状态与动作。
            if (state.puppetRequested)
            {
                const RoleStateId puppet = state.puppetState;
                state.puppetRequested  = false;
                transition(world, entity, state, puppet);
                return;
            }
            if (world.setup().mode == WorldMode::Town && !identity.playerSlot.has_value())
            {
                state.stateMs += dtMs;
                return;
            }

            // 受击族：挣脱 > 计时完成 > 追击 > 自然推进（原版 dealWithTime 先于 dealWithDoubleHit）。
            if (role::isHitState(state.state))
            {
                // 挣脱：可挣脱状态下开始了 L 槽施法 → Attack（清位移与硬直）。
                if (role::isCanBreak(state.state) && skill::rules::isCastingSlot(world, entity, "L"))
                {
                    displacement::rules::reset(world, entity);
                    hit.stiffTime       = {};
                    hit.hitTimeMs       = {};
                    hit.animationEnd    = false;
                    hit.displacementEnd = false;
                    transition(world, entity, state, RoleStateId::Attack);
                    return;
                }
                // 计时完成（Hit → HitSwitch/Death，HitFloor → GetUp/Death）先于追击。
                if (state.state == RoleStateId::Hit)
                {
                    if (updateHit(world, entity, state, hit, motion, disp, identity, dtMs))
                        return;
                }
                else if (state.state == RoleStateId::HitFloor)
                {
                    if (updateHitFloor(world, entity, state, hit, motion, disp, identity, dtMs))
                        return;
                }
                // 追击：待处理受击按当前状态分派。
                if (hit.pendingHitDefId != 0)
                {
                    switch (state.state)
                    {
                    case RoleStateId::HitSwitch:
                        transition(world, entity, state, RoleStateId::Hit);
                        enterHit(world, entity, state, hit, motion, false);
                        return;
                    case RoleStateId::Hit:
                    {
                        const auto* data = hitDef(hit.pendingHitDefId);
                        if (data)
                            chaseHitGround(world, entity, state, hit, motion, roleDef(identity), *data);
                        else
                            hit.pendingHitDefId = 0;
                        return;
                    }
                    case RoleStateId::HitUp:
                    case RoleStateId::HitDown:
                    case RoleStateId::HitFloor:
                    {
                        const auto* data = hitDef(hit.pendingHitDefId);
                        if (data)
                            chaseAir(world, entity, state, hit, motion, roleDef(identity), *data);
                        else
                            hit.pendingHitDefId = 0;
                        return;
                    }
                    default:
                        hit.pendingHitDefId = 0;
                        return;
                    }
                }
                // 自然推进（非计时状态）。
                switch (state.state)
                {
                case RoleStateId::HitSwitch:
                    updateHitSwitch(world, entity, state, hit, motion, disp, identity, dtMs);
                    return;
                case RoleStateId::HitUp:
                    updateHitUp(world, entity, state, hit, motion, disp, identity, dtMs);
                    return;
                case RoleStateId::HitDown:
                    updateHitDown(world, entity, state, hit, motion, disp, identity, dtMs);
                    return;
                case RoleStateId::GetUp:
                    updateGetUp(world, entity, state, hit, motion, disp, identity, dtMs);
                    return;
                case RoleStateId::Wake:
                    updateWake(world, entity, state, hit, motion, disp, identity, dtMs);
                    return;
                default:
                    return;
                }
            }

            // 首次受击：打断施法（Combat 已做）并进入 Hit，重设受击计数。
            if (hit.pendingHitDefId != 0)
            {
                transition(world, entity, state, RoleStateId::Hit);
                enterHit(world, entity, state, hit, motion, true);
                return;
            }

            if (identity.roleType == 1 && hit.hitProtectExecuted &&
                (state.state == RoleStateId::Attack || state.state == RoleStateId::Idle ||
                 state.state == RoleStateId::Walk || state.state == RoleStateId::Run))
            {
                skill::rules::forceInterrupt(world, entity);
                transition(world, entity, state, RoleStateId::HitDown);
                enterHitAir(world, entity, state, hit, motion, RoleStateId::HitDown);
                return;
            }

            // 6/7. 施法进入与结束。
            if (state.state == RoleStateId::Attack)
            {
                updateAttack(world, entity, state, cast, motion, intent, dtMs);
                return;
            }
            if (cast.skill != kNoSkill &&
                (state.state == RoleStateId::Idle || role::isMoveState(state.state)))
            {
                transition(world, entity, state, RoleStateId::Attack);
                return;
            }

            // 9. 移动 / 待机。
            updateLocomotion(world, entity, state, intent, motion, disp, dtMs);
        });
}
}  // namespace mugen::sim
